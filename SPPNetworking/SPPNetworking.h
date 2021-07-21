// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"

#if _WIN32 && !defined(SPP_NETWORKING_STATIC)

	#ifdef SPP_NETWORKING_EXPORT
		#define SPP_NETWORKING_API __declspec(dllexport)
	#else
		#define SPP_NETWORKING_API __declspec(dllimport)
	#endif

#else
	
	#define SPP_NETWORKING_API 

#endif

namespace SPP
{
	
}

