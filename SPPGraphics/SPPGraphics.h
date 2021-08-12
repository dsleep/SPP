// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPEngine.h"

#if _WIN32 && !defined(SPP_GRAPHICS_STATIC)
	#ifdef SPP_GRAPHICSE_EXPORT
		#define SPP_GRAPHICS_API __declspec(dllexport)
	#else
		#define SPP_GRAPHICS_API __declspec(dllimport)
	#endif
#else
	#define SPP_GRAPHICS_API 
#endif


namespace SPP
{
	SPP_GRAPHICS_API extern std::unique_ptr<class ThreadPool> GPUThreaPool;

	SPP_GRAPHICS_API void IntializeGraphics();	
	SPP_GRAPHICS_API bool IsOnGPUThread();
}
