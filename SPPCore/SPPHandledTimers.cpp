// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPHandledTimers.h"

namespace SPP
{
	using HighResClock = std::chrono::high_resolution_clock;
	using SystemClock = std::chrono::system_clock;
	using namespace std::chrono_literals;

	TimerHandle::~TimerHandle()
	{
		if (auto Lck = _parent.lock())
		{
			Lck->RemoveTimer(_timerID);
		}
	}

	void TimerController::RunOnce()
	{
		std::lock_guard<std::mutex> lk(_timer_mutex);

		auto now_us = std::chrono::duration_cast<std::chrono::microseconds> (HighResClock::now().time_since_epoch()).count();

		bool bHasUpdate = false;
		for (auto TimeIter = _timers.begin(); TimeIter != _timers.end();)
		{
			if (now_us >= (*TimeIter)->NextTrigger().count())
			{
				bHasUpdate = true;

				if ((*TimeIter->get())() == false)
				{
					TimeIter = _timers.erase(TimeIter);
				}
				else
				{
					TimeIter++;
				}
			}
			else
			{
				break;
			}
		}

		// TODO: improve this if ever more than like 5 tasks...
		if (bHasUpdate)
		{
			_timers.sort(TimerController::CompareTimedTask);
		}
	}

	void TimerController::Run()
	{
		using namespace std::chrono_literals;
		_IsRunning = true;

		while (_IsRunning)
		{
			RunOnce();

			auto now_us = std::chrono::duration_cast<std::chrono::microseconds> (HighResClock::now().time_since_epoch());
			auto SleepAmount = _sleepGranularity;

			if (!_timers.empty())
			{
				SleepAmount = std::clamp<std::chrono::microseconds>(_timers.front()->NextTrigger() - now_us, 1us, SleepAmount);
			}

			std::this_thread::sleep_for(SleepAmount);
		}
	}

	bool TimerController::RemoveTimer(int32_t TimerIn)
	{
		std::lock_guard<std::mutex> lk(_timer_mutex);

		for (auto TimeIter = _timers.begin(); TimeIter != _timers.end();)
		{
			if (TimerIn == (*TimeIter)->TimerID())
			{
				_timers.erase(TimeIter);
				return true;
			}
		}

		return false;
	}

	TimerController::~TimerController()
	{
		_IsRunning = false;

		std::lock_guard<std::mutex> lk(_timer_mutex);
		_timers.clear();
	}
}
