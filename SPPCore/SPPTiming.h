// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"

#include <chrono>
#include <condition_variable>
#include <functional> 
#include <future> 
#include <list>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits> 
#include <vector>

namespace SPP
{
	using HighResClock = std::chrono::high_resolution_clock;
	using SystemClock = std::chrono::system_clock;
	using namespace std::chrono_literals;

	template <typename T>
	class ConcurrentQueue
	{
	public:

		template<typename WaitType>
		bool pop(T& item, const WaitType &InWait)
		{
			std::unique_lock<std::mutex> mlock(_mutex);
			if (this->_cond.wait_for(mlock, InWait, [this] { return !this->_queue.empty(); }))
			{
				item = _queue.front();
				_queue.pop();
				return true;
			}
			
			return false;
		}

		void push(T&& item)
		{
			{
				std::unique_lock<std::mutex> mlock(_mutex);
				_queue.push(std::move(item));
			}		
			_cond.notify_one();
		}
		void push(T& item)
		{
			{
				std::unique_lock<std::mutex> mlock(_mutex);
				_queue.push(item);
			}
			_cond.notify_one();
		}
		size_t size()
		{
			std::unique_lock<std::mutex> mlock(_mutex);
			return _queue.size();
		}

	private:
		std::queue<T> _queue;
		std::mutex _mutex;
		std::condition_variable _cond;
	};

	//EX:  PolledRepeatingTimer< std::chrono::milliseconds >
	template<class Resolution>
	class SimplePolledRepeatingTimer
	{
	private:
		using HighResClock = std::chrono::high_resolution_clock;

		std::function<void()> _Function;
		double _RepeatTime;
		HighResClock::time_point _StartTime;

	public:

		void Initialize(std::function<void()> InFunction, double InRepeatTime)
		{
			_RepeatTime = InRepeatTime;
			_Function = InFunction;
			_StartTime = HighResClock::now();
		}

		void Reset()
		{
			_StartTime = HighResClock::now();
		}

		void Poll()
		{
			if (!_Function) return;
			const auto end = HighResClock::now();

			if (std::chrono::duration_cast<Resolution>(end - _StartTime).count() > _RepeatTime)
			{
				_Function();
				_StartTime = end;
			}
		}
	};

	SPP_CORE_API double TimeSinceAppStarted();
}