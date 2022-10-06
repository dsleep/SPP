// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPSerialization.h"

#if _WIN32 && !defined(SPP_PYTHON_STATIC)

	#ifdef SPP_PYTHON_EXPORT
		#define SPP_PYTHON_API __declspec(dllexport)
	#else
		#define SPP_PYTHON_API __declspec(dllimport)
	#endif

#else

	#define SPP_PYTHON_API

#endif

namespace SPP
{

	SPP_PYTHON_API uint32_t GetPythonVersion();
}
