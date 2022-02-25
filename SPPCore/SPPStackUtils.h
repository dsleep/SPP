// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"

#if PLATFORM_WINDOWS

namespace SPP
{
	SPP_CORE_API uint32_t DumpStackTrace(_EXCEPTION_POINTERS* ep);
}

	#define PLATFORM_TRY __try	
	#define PLATFORM_CATCH_AND_DUMP_TRACE __except (DumpStackTrace(GetExceptionInformation())) {}
#else
	#define PLATFORM_TRY 
	#define PLATFORM_CATCH_AND_DUMP_TRACE 
#endif