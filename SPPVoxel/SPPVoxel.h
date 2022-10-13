// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPEngine.h"

#if _WIN32 && !defined(SPP_VOXEL_STATIC)
	#ifdef SPP_VOXEL_EXPORT
		#define SPP_VOXEL_API __declspec(dllexport)
	#else
		#define SPP_VOXEL_API __declspec(dllimport)
	#endif
#else
	#define SPP_VOXEL_API 
#endif

namespace SPP
{
	SPP_VOXEL_API int32_t GetVoxelVersion();
}