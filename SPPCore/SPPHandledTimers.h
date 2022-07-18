// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"

#include <sstream>
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
#include <iomanip>

namespace SPP
{
	using HighResClock = std::chrono::high_resolution_clock;
	using SystemClock = std::chrono::system_clock;
	using namespace std::chrono_literals;

	class SPP_CORE_API STDElapsedTimer
	{
	private:
		std::chrono::high_resolution_clock::time_point _lastTime;

	public:
		STDElapsedTimer();
		float getElapsedSeconds();
		float getElapsedMilliseconds();
	};

	class SPP_CORE_API TimerHandle
	{
	private:
		std::weak_ptr< class TimerController > _parent;
		int32_t _timerID;

	public:
		TimerHandle(std::weak_ptr< class TimerController > Parent, int32_t TimerID) : _parent(Parent), _timerID(TimerID) {}
		~TimerHandle();
	};

	class SPP_CORE_API TimerController : public std::enable_shared_from_this<TimerController>
	{
	private:
		//_task_container_base exists only to serve as an abstract base for _task_container.
		class _timed_task_container_base
		{
		protected:
			std::chrono::microseconds _waitTime;
			std::chrono::microseconds _nextTrigger;
			int32_t _timerID;
			bool _bLooping;

		public:
			int32_t TimerID() { return _timerID; }
			const std::chrono::microseconds& NextTrigger() const { return _nextTrigger; }

			_timed_task_container_base(std::chrono::microseconds&& InWaitTime,
				std::chrono::microseconds&& InNextTrigger,
				int32_t InTimerID,
				bool bInLooped) :
				_waitTime(std::forward<std::chrono::microseconds>(InWaitTime)),
				_nextTrigger(std::forward<std::chrono::microseconds>(InNextTrigger)),
				_timerID(InTimerID),
				_bLooping(bInLooped) {};

			virtual ~_timed_task_container_base() {};
			virtual bool operator()() = 0;
		};

		template <typename F>
		class _timed_task_container : public _timed_task_container_base
		{
		private:

		public:
			//here, std::forward is needed because we need the construction of _f *not* to
			//  bind an lvalue reference - it is not a guarantee that an object of type F is
			//  CopyConstructible, only that it is MoveConstructible.
			_timed_task_container(F&& func,
				std::chrono::microseconds&& InWaitTime,
				std::chrono::microseconds&& InNextTrigger,
				int32_t InTimerID,
				bool bInLooped) : _timed_task_container_base(std::forward<std::chrono::microseconds>(InWaitTime), std::forward<std::chrono::microseconds>(InNextTrigger), InTimerID, bInLooped), _f(std::forward<F>(func))
			{
			}

			bool operator()() override
			{
				HighResClock::time_point TimerStart = HighResClock::now();
				_f();
				
				if (_bLooping)
				{
					//what if _f is longer?
					_nextTrigger = std::chrono::duration_cast<std::chrono::microseconds>(TimerStart.time_since_epoch()) + _waitTime;
				}

				return _bLooping;
			}

		private:
			F _f;
		};

		//returns a unique_ptr to a _task_container that wraps around a given function
		//  for details on _task_container_base and _task_container, see above
		//  This exists so that _Func may be inferred from f.
		template <typename _Func>
		static std::unique_ptr<_timed_task_container_base> allocate_timed_task_container(_Func&& f,
			std::chrono::microseconds&& InWaitTime,
			std::chrono::microseconds&& InNextTrigger,
			int32_t InTimerID,
			bool bInLooped)
		{
			//in the construction of the _task_container, f must be std::forward'ed because
			//  it may not be CopyConstructible - the only requirement for an instantiation
			//  of a _task_container is that the parameter is of a MoveConstructible type.
			return std::unique_ptr<_timed_task_container_base>(
				new _timed_task_container<_Func>(std::forward<_Func>(f),
					std::forward<std::chrono::microseconds>(InWaitTime),
					std::forward<std::chrono::microseconds>(InNextTrigger),
					InTimerID,
					bInLooped)
				);
		}

		std::list<std::unique_ptr<_timed_task_container_base>> _timers;
		std::mutex _timer_mutex;
		std::atomic_bool _IsRunning = false;
		std::chrono::microseconds _sleepGranularity{ 0 };

		int32_t TimerCounter = 0;

		TimerController(const TimerController&) = delete;
		TimerController& operator=(const TimerController&) = delete;

		static bool CompareTimedTask(const std::unique_ptr<_timed_task_container_base >& first, const std::unique_ptr<_timed_task_container_base>& second)
		{
			return (first->NextTrigger().count() < second->NextTrigger().count());
		}

	public:

		template <class _Rep, class _Period>
		TimerController(const std::chrono::duration<_Rep, _Period>& SleepGranularity) : _sleepGranularity(SleepGranularity) {}
		~TimerController();
				
		void RunOnce();
		void Run();

		void Stop()
		{
			_IsRunning = false;
		}

		//F must be Callable, and invoking F with ...Args must be well-formed.
		template <typename D, typename F, typename ...Args>
		int32_t AddTimer(D duration, bool bInIsLooping, F function, Args &&...args);

		template <typename D, typename F, typename ...Args>
		std::unique_ptr< TimerHandle > AddTimerHandled(D duration, bool bInIsLooping, F function, Args &&...args);

		bool RemoveTimer(int32_t TimerIn);
	};

	template <typename D, typename F, typename ...Args>
	int32_t TimerController::AddTimer(D duration, bool bInIsLooping, F function, Args &&...args)
	{
		std::function<std::invoke_result_t<F, Args...>()> task_pkg(std::bind(function, args...));

		int32_t ThisCounter = 0;

		{
			std::lock_guard<std::mutex> lk(_timer_mutex);
			auto TimerStart = HighResClock::now().time_since_epoch();

			ThisCounter = TimerCounter++;

			//this lambda move-captures the packaged_task declared above. Since the packaged_task
			//  type is not CopyConstructible, the function is not CopyConstructible either -
			//  hence the need for a _task_container to wrap around it.
			_timers.emplace_back(
				allocate_timed_task_container(
					[task(std::move(task_pkg))]() mutable { task(); },
					std::chrono::duration_cast<std::chrono::microseconds>(duration),
					std::chrono::duration_cast<std::chrono::microseconds>(TimerStart + duration),
					ThisCounter,
					bInIsLooping
				)
			);

			_timers.sort(TimerController::CompareTimedTask);
		}

		return ThisCounter;
	}

	template <typename D, typename F, typename ...Args>
	std::unique_ptr< TimerHandle > TimerController::AddTimerHandled(D duration, bool bInIsLooping, F function, Args &&...args)
	{
		return std::make_unique< TimerHandle >(shared_from_this(), AddTimer(duration, bInIsLooping, function, args...));
	}
}
