// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
	
namespace SPP
{
	struct PlatformInfo
	{
		uint32_t PageSize;
		uint32_t ProcessorCount;
	};

	SPP_CORE_API PlatformInfo GetPlatformInfo();
	SPP_CORE_API uint32_t CreateChildProcess(const char* ProcessPath, const char* Commandline);
	SPP_CORE_API bool IsChildRunning(uint32_t processID);
	SPP_CORE_API void CloseChild(uint32_t processID);

	SPP_CORE_API void AddDLLSearchPath(const char* InPath);
}

extern "C" SPP_CORE_API uint32_t C_CreateChildProcess(const char* ProcessPath, const char* Commandline);
extern "C" SPP_CORE_API bool C_IsChildRunning(uint32_t processID);
extern "C" SPP_CORE_API void C_CloseChild(uint32_t processID);