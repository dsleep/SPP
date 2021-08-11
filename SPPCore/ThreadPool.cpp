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

// Windows Header Files
#if _WIN32
	#include <windows.h>
#endif

namespace SPP
{
	ThreadPool::ThreadPool(uint8_t threads) : stop(false)
	{
		for (size_t i = 0; i < threads; ++i)
			workers.emplace_back(
				[this]
				{
					for (;;)
					{
						std::function<void()> task;

						{
							std::unique_lock<std::mutex> lock(this->queue_mutex);
							this->condition.wait(lock,
								[this] { return this->stop || !this->tasks.empty(); });
							if (this->stop && this->tasks.empty())
								return;
													   
							task = std::move(this->tasks.front());
							this->tasks.pop();
						}

						{
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

#if _WIN32
	uint32_t GetCurrentThreadID()
	{
		return ::GetCurrentThreadId();
	}
#endif
}
