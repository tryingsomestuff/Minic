#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <optional>
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

struct BasicSfenInputStream
{
	virtual std::optional<PackedSfenValue> next() = 0;
	virtual bool eof() const = 0;
	virtual ~BasicSfenInputStream() {}
};

struct BinSfenInputStream : BasicSfenInputStream
{
	static constexpr auto openmode = std::ios::in | std::ios::binary;
	static inline const std::string extension = "bin";

	BinSfenInputStream(std::string filename) :
		m_stream(filename, openmode),
		m_eof(!m_stream)
	{
	}

	std::optional<PackedSfenValue> next() override
	{
		PackedSfenValue e;
		if(m_stream.read(reinterpret_cast<char*>(&e), sizeof(PackedSfenValue)))
		{
			return e;
		}
		else
		{
			m_eof = true;
			return std::nullopt;
		}
	}

	bool eof() const override
	{
		return m_eof;
	}

	~BinSfenInputStream() override {}

private:
	std::fstream m_stream;
	bool m_eof;
};

inline std::unique_ptr<BasicSfenInputStream> open_sfen_input_file(const std::string& filename)
{
    return std::make_unique<BinSfenInputStream>(filename);
}

// Sfen reader
struct SfenReader
{
	// Number of phases buffered by each thread 0.1M phases. 4M phase at 40HT
	static constexpr size_t THREAD_BUFFER_SIZE = 10 * 1000;

	// Buffer for reading files (If this is made larger,
	// the shuffle becomes larger and the phases may vary.
	// If it is too large, the memory consumption will increase.
	// SFEN_READ_SIZE is a multiple of THREAD_BUFFER_SIZE.
	static constexpr const size_t SFEN_READ_SIZE = LEARN_SFEN_READ_SIZE;

	// hash to limit the reading of the same situation
	// Is there too many 64 million phases? Or Not really..
	// It must be 2**N because it will be used as the mask to calculate hash_index.
	static constexpr uint64_t READ_SFEN_HASH_SIZE = 64 * 1024 * 1024;

	// Do not use std::random_device().
	// Because it always the same integers on MinGW.
	SfenReader(int thread_num, const std::string& seed) :
		prng(seed)
	{
		packed_sfens.resize(thread_num);
		stop_flag = false;
	}

	~SfenReader()
	{
		if (file_worker_thread.joinable())
			file_worker_thread.join();
	}

	// [ASYNC] Thread returns one aspect. Otherwise returns false.
	bool read_to_thread_buffer(size_t thread_id, PackedSfenValue& ps)
	{
		// If there are any positions left in the thread buffer
		// then retrieve one and return it.
		auto& thread_ps = packed_sfens[thread_id];

		// Fill the read buffer if there is no remaining buffer,
		// but if it doesn't even exist, finish.
		// If the buffer is empty, fill it.
		if ((thread_ps == nullptr || thread_ps->empty())
			&& !read_to_thread_buffer_impl(thread_id))
			return false;

		// read_to_thread_buffer_impl() returned true,
		// Since the filling of the thread buffer with the
		// phase has been completed successfully
		// thread_ps->rbegin() is alive.

		ps = thread_ps->back();
		thread_ps->pop_back();

		// If you've run out of buffers, call delete yourself to free this buffer.
		if (thread_ps->empty())
		{
			thread_ps.reset();
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

					packed_sfens[thread_id] = std::move(packed_sfens_pool.front());
					packed_sfens_pool.pop_front();

					return true;
				}
			}

			// Waiting for file worker to fill packed_sfens_pool.
			// The mutex isn't locked, so it should fill up soon.
			// Poor man's condition variable.
			sleep(1);
		}

	}

	std::atomic<bool> stop_flag;

	// test phase for mse calculation
	PSVector sfen_for_mse;

protected:

	// worker thread reading file in background
	std::thread file_worker_thread;

	// Random number to shuffle when reading the phase
	FromSF::PRNG prng;

	// sfen for each thread
	// (When the thread is used up, the thread should call delete to release it.)
	std::vector<std::unique_ptr<PSVector>> packed_sfens;

	// Mutex when accessing packed_sfens_pool
	std::mutex mutex;

	// pool of sfen. The worker thread read from the file is added here.
	// Each worker thread fills its own packed_sfens[thread_id] from here.
	// * Lock and access the mutex.
	std::list<std::unique_ptr<PSVector>> packed_sfens_pool;

};
