#include "definition.hpp"

#ifdef WITH_LEARNER

#include <thread>

#include "multi_think.h"

#include "dynamicConfig.hpp"
#include "nnue.hpp"

void MultiThink::go_think()
{

	// Read evaluation function, etc.
	NNUEWrapper::init_NNUE();

	// Call the derived class's init().
	init();

	// The loop upper limit is set with set_loop_max().
	loop_count = 0;
	done_count = 0;

	// Create threads as many as Options["Threads"] and start thinking.
	std::vector<std::thread> threads;
	auto thread_num = DynamicConfig::threads;

	// Secure end flag of worker thread
	thread_finished.resize(thread_num);
	
	// start worker thread
	for (size_t i = 0; i < thread_num; ++i)
	{
		thread_finished[i] = 0;
		threads.push_back(std::thread([i, this]
		{ 
			// execute the overridden process
			this->thread_worker(i);
			// Set the end flag because the thread has ended
			this->thread_finished[i] = 1;
		}));
	}

	// function to determine if all threads have finished
	auto threads_done = [&]()
	{
		// returns false if no one is finished
		for (auto& f : thread_finished)
			if (!f)
				return false;
		return true;
	};

	// Call back if the callback function is set.
	auto do_a_callback = [&]()
	{
		if (callback_func)
			callback_func();
	};

	for (uint64_t i = 0 ; ; )
	{
		// If all threads have finished, exit the loop.
		if (threads_done())
			break;

		sleep(1000);

		// callback_func() is called every callback_seconds.
		if (++i == callback_seconds)
		{
			do_a_callback();
			i = 0;
		}
	}

	// Last save.
	std::cout << std::endl << "finalize..";

	// It is possible that the exit code of the thread is running but the exit code of the thread is running, so
	// We need to wait for the end with join().
	for (auto& th : threads)
		th.join();

	// The file writing thread etc. are still running only when all threads are finished
	// Since the work itself may not have completed, output only that all threads have finished.
	std::cout << "all threads are joined." << std::endl;

}

#endif