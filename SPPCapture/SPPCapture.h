// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <vector>

#if _WIN32 && !defined(SPP_CAPTURE_STATIC)

	#ifdef SPP_CAPTURE_EXPORT
		#define SPP_CAPTURE_API __declspec(dllexport)
	#else
		#define SPP_CAPTURE_API __declspec(dllimport)
	#endif

#else
	
	#define SPP_CAPTURE_API 

#endif

namespace SPP
{
	SPP_CAPTURE_API bool CaptureApplicationWindow(uint32_t ProcessID, int32_t &oWidth, int32_t &oHeight, std::vector<uint8_t> &oImageData, uint8_t &oBytesPerPixel );
}

