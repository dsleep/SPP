// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
//based off original work of Jakob Progsch, V�clav Zeman
//
//https://github.com/progschj/ThreadPool
//
//Copyright(c) 2012 Jakob Progsch, V�clav Zeman
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
		ThreadPool(const std::string &dInName, uint8_t threads, bool bInWaitForTrigger = false);		

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
			if (!bWaitForTrigger)
			{
				condition.notify_one();
			}
			return res;
		}

		size_t TaskCount();
		
		~ThreadPool();

		bool IsRunning();

		//runs all tasks just once
		void RunOnce();

		//
		void RunAll();

		size_t WorkerCount() const
		{
			return workers.size();
		}

		template <typename TIter, class F>
		void parallel_for(TIter beg, TIter end, F&& f)
		{
			auto len = end - beg;
			if (len < threadCount)
			{
				for (uint32_t Iter = 0; Iter < len; Iter++)
				{
					TIter curEle = beg + Iter;
					f(curEle);
				}
			}

			auto batch_size = (len + (threadCount-1)) / threadCount;

			std::list<std::future<void>> futures;

			for (uint32_t Iter = 0; Iter < threadCount; Iter++)
			{
				auto startIdx = (Iter * batch_size);
				if (startIdx >= len)
				{
					break;
				}
				auto curStart = beg + startIdx;
				auto curEnd = beg + std::min( ((Iter+1) * batch_size), len );

				futures.push_back(enqueue([curStart, curEnd, f]() {
						for (auto Iter = curStart; Iter < curEnd; Iter++)
						{
							f(Iter);
						}
					}));
			}

			for (auto& curFuture : futures)
			{
				curFuture.wait();
			}
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

		uint8_t threadCount;
		//
		bool bRunning;
		bool bWaitForTrigger;
	};

	inline float TimeSince(const std::chrono::high_resolution_clock::time_point& InTime)
	{
		return std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now() - InTime).count();
	}

	SPP_CORE_API uint32_t GetCurrentThreadID();
}
