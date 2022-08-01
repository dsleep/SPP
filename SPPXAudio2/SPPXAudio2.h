// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPMath.h"

#if _WIN32 && !defined(SPP_XAUDIO2_STATIC)

	#ifdef SPP_XAUDIO2_EXPORT
		#define SPP_XAUDIO2_API __declspec(dllexport)
	#else
		#define SPP_XAUDIO2_API __declspec(dllimport)
	#endif

#else

	#define SPP_XAUDIO2_API 

#endif

namespace SPP
{		
	SPP_XAUDIO2_API uint32_t GetXAudio2Version();
	
}
