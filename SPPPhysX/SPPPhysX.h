// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPReferenceCounter.h"
#include <coroutine>
#include <list>

#if _WIN32 && !defined(SPP_PHYSX_STATIC)

	#ifdef SPP_PHYSX_EXPORT
		#define SPP_PHYSX_API __declspec(dllexport)
	#else
		#define SPP_PHYSX_API __declspec(dllimport)
	#endif

#else

	#define SPP_PHYSX_API 

#endif

namespace SPP
{		
	SPP_PHYSX_API uint32_t GetPhysXVersion();
	SPP_PHYSX_API void InitializePhysX();
}
