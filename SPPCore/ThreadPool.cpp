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

#include "ThreadPool.h"

#include "SPPPlatformCore.h"
#include "SPPString.h"
#include "SPPProfiler.h"

// Windows Header Files
#if _WIN32
	#include <windows.h>
#endif

namespace SPP
{
	ThreadPool::ThreadPool(const std::string& InName, uint8_t threads, bool bInWaitForTrigger) : threadCount(threads), stop(false), bWaitForTrigger(bInWaitForTrigger), bRunning(!bInWaitForTrigger)
	{
		for (size_t i = 0; i < threads; ++i)
			workers.emplace_back(
				[this, InName, i]
				{
					std::string threadName = std::string_format("%s_%d", InName.c_str(), i);
					SetThreadName(threadName.c_str());
					ProfilerThreadRegister regThread(threadName.c_str());
					for (;;)
					{
						std::function<void()> task;

						{
							std::unique_lock<std::mutex> lock(this->queue_mutex);
							this->condition.wait(lock,
								[this] 
								{
									return (this->stop) || (bRunning && !this->tasks.empty());
								});
							if (this->stop && this->tasks.empty())
								return;							
													   
							task = std::move(this->tasks.front());
							this->tasks.pop();

							// we got the last task for the trigger
							if (this->bWaitForTrigger && this->tasks.empty())
							{
								bRunning = false;
							}
						}

						{
							P_SCOPE("TASK");
							task();
						}
					}
				}
				);
	}

	void ThreadPool::RunOnce()
	{
		std::queue< std::function<void()> > currenttasks;
		
		{
			std::unique_lock<std::mutex> lock(this->queue_mutex);
			std::swap(currenttasks, tasks);
		}

		while (currenttasks.size() > 0)
		{
			std::function<void()> currenttask = std::move(currenttasks.front());
			currenttask();
			currenttasks.pop();
		}
	}	

	void ThreadPool::RunAll()
	{
		{
			std::unique_lock<std::mutex> lock(this->queue_mutex);
			bRunning = true;
		}
		condition.notify_all();
	}

	bool ThreadPool::IsRunning()
	{
		std::unique_lock<std::mutex> lock(this->queue_mutex);
		return bRunning;
	}


	size_t ThreadPool::TaskCount()
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		return tasks.size();
	}

	ThreadPool::~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			stop = true;
		}
		condition.notify_all();
		for (std::thread& worker : workers)
			worker.join();
	}

#if _WIN32
	uint32_t GetCurrentThreadID()
	{
		return ::GetCurrentThreadId();
	}
#endif
}
