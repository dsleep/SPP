// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPMath.h"

#if _WIN32 && !defined(SPP_ANIMATION_STATIC)

	#ifdef SPP_ANIMATION_EXPORT
		#define SPP_ANIMATION_API __declspec(dllexport)
	#else
		#define SPP_ANIMATION_API __declspec(dllimport)
	#endif

#else

	#define SPP_ANIMATION_API 

#endif

namespace SPP
{		
	SPP_ANIMATION_API uint32_t GetAnimationVersion();
	SPP_ANIMATION_API void InitializeAnimation();
	SPP_ANIMATION_API void* LoadSkeleton(const char* FilePath);
	SPP_ANIMATION_API void* LoadAnimations(const char* FilePath);
}
