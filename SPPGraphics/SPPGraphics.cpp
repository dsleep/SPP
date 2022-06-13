// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"

#include "SPPLogging.h"
#include "ThreadPool.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	LogEntry LOG_GRAPHICS("GRAPHICS");

	SPP_GRAPHICS_API std::unique_ptr<ThreadPool> GPUThreaPool;

	static std::thread::id GPUThread;

	void IntializeGraphics()
	{
		SPP_LOG(LOG_GRAPHICS, LOG_INFO, "IntializeGraphics");
		GPUThreaPool = std::make_unique< ThreadPool >(1);
		auto isSet = GPUThreaPool->enqueue([]()
			{
				GPUThread = std::this_thread::get_id();
			});
		isSet.wait();
	}

	bool IsOnGPUThread()
	{
		// make sure its an initialized one
		SE_ASSERT(GPUThread != std::thread::id());
		auto currentThreadID = std::this_thread::get_id();
		return (GPUThread == currentThreadID);
	}

	GPUThreadIDOverride::GPUThreadIDOverride()
	{
		prevID = GPUThread;
		GPUThread = std::this_thread::get_id();
	}
	GPUThreadIDOverride::~GPUThreadIDOverride()
	{
		GPUThread = prevID;
	}

	GPU_CALL gpu_coroutine_promise::get_return_object() noexcept
	{
		return GPU_CALL(coro_handle::from_promise(*this));
	}

	GPU_CALL::GPU_CALL(coro_handle InHandle)
	{
		if (IsOnGPUThread())
		{
			InHandle.resume();
			SE_ASSERT(InHandle.done());
		}
		else
		{
			GPUThreaPool->enqueue([InHandle]()
				{
					InHandle.resume();
					SE_ASSERT(InHandle.done());
				});
		}
	}
}