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
	class SPP_CORE_API IPCMappedMemory
	{
	private:
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

	public:		
		IPCMappedMemory(const char* MappedName, size_t MemorySize, bool bIsNew);
		~IPCMappedMemory();

		bool IsValid() const;
		void WriteMemory(const void* InMem, size_t DataSize, size_t Offset = 0);
		void ReadMemory(void* OutMem, size_t DataSize, size_t Offset = 0);
	};
}