// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <string>

#if _WIN32 && !defined(SPP_GAMEENGINE_STATIC)

	#ifdef SPP_GAMEENGINE_EXPORT
		#define SPP_GAMEENGINE_API __declspec(dllexport)
	#else
		#define SPP_GAMEENGINE_API __declspec(dllimport)
	#endif

	#else

		#define SPP_GAMEENGINE_API 

#endif



namespace SPP
{
	SPP_GAMEENGINE_API uint32_t GetGameEngineVersion();
}

