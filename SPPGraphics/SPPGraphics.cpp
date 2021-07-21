// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"

#include "SPPLogging.h"
#include "ThreadPool.h"

namespace SPP
{
	LogEntry LOG_GRAPHICS("GRAPHICS");

	SPP_GRAPHICS_API std::unique_ptr<ThreadPool> GPUThreaPool;

	static std::thread::id GPUThread;

	void IntializeGraphics()
	{
		SPP_LOG(LOG_GRAPHICS, LOG_INFO, "IntializeGraphics");
		GPUThreaPool = std::make_unique< ThreadPool >(1);
		GPUThreaPool->enqueue([]()
			{
				GPUThread = std::this_thread::get_id();
			});
	}

	bool IsOnGPUThread()
	{
		// make sure its an initialized one
		SE_ASSERT(GPUThread != std::thread::id());
		auto currentThreadID = std::this_thread::get_id();
		return (GPUThread == currentThreadID);
	}
}