// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
//based off original work of Jakob Progsch, Václav Zeman
//
//https://github.com/progschj/ThreadPool
//
//Copyright(c) 2012 Jakob Progsch, Václav Zeman
//
//This software is provided 'as-is', without any express or implied
//warranty.In no event will the authors be held liable for any damages
//arising from the use of this software.
//
//Permission is granted to anyone to use this software for any purpose,
//including commercial applications, and to alter itand redistribute it
//freely, subject to the following restrictions :
//
//1. The origin of this software must not be misrepresented; you must not
//claim that you wrote the original software.If you use this software
//in a product, an acknowledgment in the product documentation would be
//appreciated but is not required.
//
//2. Altered source versions must be plainly marked as such, and must not be
//misrepresented as being the original software.
//
//3. This notice may not be removed or altered from any source
//distribution.

#pragma once

#include "SPPCore.h"

#include <map>
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace SPP
{	
	class SPP_CORE_API ThreadPool : public std::enable_shared_from_this<ThreadPool>
	{
	public:		
		ThreadPool(uint8_t threads);		

		template<class F, class... Args>
		auto enqueue(F&& f, Args&&... args)->std::future< typename std::invoke_result_t<F,Args...> >
		{
			using return_type = typename std::invoke_result_t<F,Args...>;

			auto task = std::make_shared< std::packaged_task<return_type()> >(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
				);

			std::future<return_type> res = task->get_future();
			{
				std::unique_lock<std::mutex> lock(queue_mutex);

				// don't allow enqueue after stopping the pool
				if (stop)
				{
					throw std::runtime_error("enqueue on stopped ThreadPool");
				}

				tasks.emplace([task]() { (*task)(); });
			}
			condition.notify_one();
			return res;
		}

		size_t TaskCount()
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			return tasks.size();
		}
		
		~ThreadPool()
		{
			{
				std::unique_lock<std::mutex> lock(queue_mutex);
				stop = true;
			}
			condition.notify_all();
			for (std::thread &worker : workers)
				worker.join();
		}

		//runs all tasks just once
		void RunOnce();

		size_t WorkerCount() const
		{
			return workers.size();
		}

	private:
		// need to keep track of threads so we can join them
		std::vector< std::thread > workers;
		// the task queue
		std::queue< std::function<void()> > tasks;
		// synchronization
		std::mutex queue_mutex;
		std::condition_variable condition;
		bool stop;
	};

	inline float TimeSince(const std::chrono::high_resolution_clock::time_point& InTime)
	{
		return std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now() - InTime).count();
	}

	SPP_CORE_API uint32_t GetCurrentThreadID();
}
