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

	void IntializeGraphicsThread()
	{
		SPP_LOG(LOG_GRAPHICS, LOG_INFO, "IntializeGraphics");
		GPUThreaPool = std::make_unique< ThreadPool >(1);
		auto isSet = GPUThreaPool->enqueue([]()
			{
				GPUThread = std::this_thread::get_id();
			});
		isSet.wait();
	}

	void ShutdownGraphicsThread()
	{
		GPUThreaPool.reset();
	}

	bool IsOnGPUThread()
	{
		// make sure its an initialized one
		return (GPUThread == std::this_thread::get_id()) ||
			(GPUThread == std::thread::id());
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

	GraphicsDevice::~GraphicsDevice()
	{
		SE_ASSERT(_resources.empty());
	}

	void GraphicsDevice::PushResource(GPUResource* InResource)
	{
		_resources.push_back(InResource);
	}
	void GraphicsDevice::PopResource(GPUResource* InResource)
	{
		_resources.remove(InResource);
	}
}