#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <thread>
#include <unordered_set>
#include <vector>

#include "definition.hpp"

#include "learn_tools.hpp"

#include "hash.hpp"
#include "position.hpp"

#define LEARN_SFEN_READ_SIZE (1000 * 1000 * 10)

// Phase array: PSVector stands for packed sfen vector.
typedef std::vector<PackedSfenValue> PSVector;

// Defined in learner.cpp
namespace LearnerConfig{ 
	extern bool use_draw_games_in_training;
	extern bool use_draw_games_in_validation;
	// 1.0 / PawnValueEg / 4.0 * log(10.0)
	extern double winning_probability_coefficient;
	// Score scale factors.  ex) If we set src_score_min_value = 0.0,
	// src_score_max_value = 1.0, dest_score_min_value = 0.0,
	// dest_score_max_value = 10000.0, [0.0, 1.0] will be scaled to [0, 10000].
	extern double src_score_min_value;
	extern double src_score_max_value;
	extern double dest_score_min_value;
	extern double dest_score_max_value;
	// Assume teacher signals are the scores of deep searches, and convert them into winning
	// probabilities in the trainer. Sometimes we want to use the winning probabilities in the training
	// data directly. In those cases, we set false to this variable.
	extern bool convert_teacher_signal_to_winning_probability;
	// Using WDL with win rate model instead of sigmoid
	extern bool use_wdl;
}

// Sfen reader
struct SfenReader
{
	// Do not use std::random_device().  Because it always the same integers on MinGW.
	SfenReader(int thread_num) : prng(std::chrono::system_clock::now().time_since_epoch().count())
	{
		packed_sfens.resize(thread_num);
		total_read = 0;
		total_done = 0;
		last_done = 0;
		next_update_weights = 0;
		save_count = 0;
		end_of_files = false;
		no_shuffle = false;
		stop_flag = false;

		hash.resize(READ_SFEN_HASH_SIZE);
	}

	~SfenReader()
	{
		if (file_worker_thread.joinable())
			file_worker_thread.join();

		for (auto p : packed_sfens)
			delete p;
		for (auto p : packed_sfens_pool)
			delete p;
	}

	// number of phases used for calculation such as mse
	// mini-batch size = 1M is standard, so 0.2% of that should be negligible in terms of time.
	//Since search() is performed with depth = 1 in calculation of move match rate, simple comparison is not possible...
	const uint64_t sfen_for_mse_size = 2000;

	// Load the phase for calculation such as mse.
	void read_for_mse()
	{
        Position pos;
        readFEN(startPosition,pos,true);
		for (uint64_t i = 0; i < sfen_for_mse_size; ++i)
		{
			PackedSfenValue ps;
			if (!read_to_thread_buffer(0, ps))
			{
				std::cout << "Error! read packed sfen , failed." << std::endl;
				break;
			}
			sfen_for_mse.push_back(ps);

			// Get the hash key.
			set_from_packed_sfen(pos,ps.sfen,false);
			sfen_for_mse_hash.insert(computeHash(pos));
		}
	}

	void read_validation_set(const std::string file_name, int eval_limit)
	{
		std::ifstream f(file_name, std::ios::binary);

		while (f)
		{
			PackedSfenValue p;
			if (f.read((char*)&p, sizeof(PackedSfenValue)))
			{
				if (eval_limit < abs(p.score))
					continue;
				if (!LearnerConfig::use_draw_games_in_validation && p.game_result == 0)
					continue;
				sfen_for_mse.push_back(p);
			} else {
				break;
			}
		}
	}

	// Number of phases buffered by each thread 0.1M phases. 4M phase at 40HT
	const size_t THREAD_BUFFER_SIZE = 10 * 1000;

	// Buffer for reading files (If this is made larger, the shuffle becomes larger and the phases may vary.
	// If it is too large, the memory consumption will increase.
	// SFEN_READ_SIZE is a multiple of THREAD_BUFFER_SIZE.
	const size_t SFEN_READ_SIZE = LEARN_SFEN_READ_SIZE;

	// [ASYNC] Thread returns one aspect. Otherwise returns false.
	bool read_to_thread_buffer(size_t thread_id, PackedSfenValue& ps)
	{
		// If there are any positions left in the thread buffer, retrieve one and return it.
		auto& thread_ps = packed_sfens[thread_id];

		// Fill the read buffer if there is no remaining buffer, but if it doesn't even exist, finish.
		if ((thread_ps == nullptr || thread_ps->size() == 0) // If the buffer is empty, fill it.
			&& !read_to_thread_buffer_impl(thread_id))
			return false;

		// read_to_thread_buffer_impl() returned true,
		// Since the filling of the thread buffer with the phase has been completed successfully
		// thread_ps->rbegin() is alive.

		ps = *(thread_ps->rbegin());
		thread_ps->pop_back();

		// If you've run out of buffers, call delete yourself to free this buffer.
		if (thread_ps->size() == 0)
		{

			delete thread_ps;
			thread_ps = nullptr;
		}

		return true;
	}

	// [ASYNC] Read some aspects into thread buffer.
	bool read_to_thread_buffer_impl(size_t thread_id)
	{
		while (true)
		{
			{
				std::unique_lock<std::mutex> lk(mutex);
				// If you can fill from the file buffer, that's fine.
				if (packed_sfens_pool.size() != 0)
				{
					// It seems that filling is possible, so fill and finish.

					packed_sfens[thread_id] = packed_sfens_pool.front();
					packed_sfens_pool.pop_front();

					total_read += THREAD_BUFFER_SIZE;

					return true;
				}
			}

			// The file to read is already gone. No more use.
			if (end_of_files)
				return false;

			// Waiting for file worker to fill packed_sfens_pool.
			// The mutex isn't locked, so it should fill up soon.
			sleep(1);
		}

	}

	// Start a thread that loads the phase file in the background.
	void start_file_read_worker()
	{
		file_worker_thread = std::thread([&] { this->file_read_worker(); });
	}

	// for file read-only threads
	void file_read_worker()
	{
		auto open_next_file = [&]()
		{
			if (fs.is_open())
				fs.close();

			// no more
			if (filenames.size() == 0)
				return false;

			// Get the next file name.
			std::string filename = *filenames.rbegin();
			filenames.pop_back();

			fs.open(filename, std::ios::in | std::ios::binary);
			std::cout << "open filename = " << filename << std::endl;
			assert(fs);

			return true;
		};

		while (true)
		{
			// Wait for the buffer to run out.
			// This size() is read only, so you don't need to lock it.
			while (!stop_flag && packed_sfens_pool.size() >= SFEN_READ_SIZE / THREAD_BUFFER_SIZE)
				sleep(100);
			if (stop_flag)
				return;

			PSVector sfens;
			sfens.reserve(SFEN_READ_SIZE);

			// Read from the file into the file buffer.
			while (sfens.size() < SFEN_READ_SIZE)
			{
				PackedSfenValue p;
				if (fs.read((char*)&p, sizeof(PackedSfenValue)))
				{
					sfens.push_back(p);
				} else
				{
					// read failure
					if (!open_next_file())
					{
						// There was no next file. Abon.
						std::cout << "..end of files." << std::endl;
						end_of_files = true;
						return;
					}
				}
			}

			// Shuffle the read phase data.
			// random shuffle by Fisher-Yates algorithm

			if (!no_shuffle)
			{
				auto size = sfens.size();
				for (size_t i = 0; i < size; ++i)
					std::swap(sfens[i], sfens[(size_t)(prng.rand((uint64_t)size - i) + i)]);
			}

			// Divide this by THREAD_BUFFER_SIZE. There should be size pieces.
			// SFEN_READ_SIZE shall be a multiple of THREAD_BUFFER_SIZE.
			assert((SFEN_READ_SIZE % THREAD_BUFFER_SIZE)==0);

			auto size = size_t(SFEN_READ_SIZE / THREAD_BUFFER_SIZE);
			std::vector<PSVector*> ptrs;
			ptrs.reserve(size);

			for (size_t i = 0; i < size; ++i)
			{
				// Delete this pointer on the receiving side.
				PSVector* ptr = new PSVector();
				ptr->resize(THREAD_BUFFER_SIZE);
				memcpy(&((*ptr)[0]), &sfens[i * THREAD_BUFFER_SIZE], sizeof(PackedSfenValue) * THREAD_BUFFER_SIZE);

				ptrs.push_back(ptr);
			}

			// Since sfens is ready, look at the occasion and copy
			{
				std::unique_lock<std::mutex> lk(mutex);

				// You can ignore this time because you just copy the pointer...
				// The mutex lock is required because the contents of packed_sfens_pool are changed.

				for (size_t i = 0; i < size; ++i)
					packed_sfens_pool.push_back(ptrs[i]);
			}
		}
	}

	// sfen files
	std::vector<std::string> filenames;

	// number of phases read (file to memory buffer)
	std::atomic<uint64_t> total_read;

	// number of processed phases
	std::atomic<uint64_t> total_done;

	// number of cases processed so far
	uint64_t last_done;

	// If total_read exceeds this value, update_weights() and calculate mse.
	uint64_t next_update_weights;

	uint64_t save_count;

	// Do not shuffle when reading the phase.
	bool no_shuffle;

	bool stop_flag;

	// Determine if it is a phase for calculating rmse.
	// (The computational aspects of rmse should not be used for learning.)
	bool is_for_rmse(Hash key) const
	{
			return sfen_for_mse_hash.count(key) != 0;
	}

	// hash to limit the reading of the same situation
	// Is there too many 64 million phases? Or Not really..
	// It must be 2**N because it will be used as the mask to calculate hash_index.
	static const uint64_t READ_SFEN_HASH_SIZE = 64 * 1024 * 1024;
	std::vector<Hash> hash; // 64MB*8 = 512MB

	// test phase for mse calculation
	PSVector sfen_for_mse;

protected:

	// worker thread reading file in background
	std::thread file_worker_thread;

	// Random number to shuffle when reading the phase
	FromSF::PRNG prng;

	// Did you read the files and reached the end?
	std::atomic<bool> end_of_files;


	// handle of sfen file
	std::fstream fs;

	// sfen for each thread
	// (When the thread is used up, the thread should call delete to release it.)
	std::vector<PSVector*> packed_sfens;

	// Mutex when accessing packed_sfens_pool
	std::mutex mutex;

	// pool of sfen. The worker thread read from the file is added here.
	// Each worker thread fills its own packed_sfens[thread_id] from here.
	// * Lock and access the mutex.
	std::list<PSVector*> packed_sfens_pool;

	// Hold the hash key so that the mse calculation phase is not used for learning.
	std::unordered_set<Hash> sfen_for_mse_hash;
};
