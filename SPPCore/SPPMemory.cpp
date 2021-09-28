// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPMemory.h"
#include "SPPLogging.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

namespace SPP
{
	LogEntry LOG_IPC("IPC");

	LogEntry LOG_MEM("MEM");

	struct IPCMappedMemory::PlatImpl
	{
		HANDLE hMapFile = nullptr;
		uint8_t* dataLink = nullptr;
		HANDLE hFileMutex = nullptr;
	};

	IPCMappedMemory::IPCMappedMemory(const char* MappedName, size_t MemorySize, bool bIsNew) : _impl(new PlatImpl()), _memorySize(MemorySize)
	{
		SPP_LOG(LOG_IPC, LOG_INFO, "IPCMappedMemory::IPCMappedMemory: (%s:%d) %d", MappedName, bIsNew, MemorySize);

		if (bIsNew)
		{
			_impl->hMapFile = CreateFileMappingA(
				INVALID_HANDLE_VALUE,    // use paging file
				NULL,                    // default security
				PAGE_READWRITE,          // read/write access
				0,                       // maximum object size (high-order DWORD)
				(DWORD)MemorySize,                // maximum object size (low-order DWORD)
				MappedName);                 // name of mapping object
		}
		else
		{
			_impl->hMapFile = OpenFileMappingA(
				FILE_MAP_ALL_ACCESS,   // read/write access
				FALSE,                 // do not inherit the name
				MappedName);               // name of mapping object
		}

		SE_ASSERT(_impl->hMapFile);
		SPP_LOG(LOG_IPC, LOG_INFO, "IPCMappedMemory::IPCMappedMemory: has Link");

		_impl->dataLink = (uint8_t*)MapViewOfFile(_impl->hMapFile,   // handle to map object
			FILE_MAP_ALL_ACCESS, // read/write permission
			0,
			0,
			MemorySize);


		std::string MutexName = std::string(MappedName) + "_M";

		if (bIsNew)
		{
			memset(_impl->dataLink, 0, MemorySize);

			_impl->hFileMutex = CreateMutexA(
				NULL,
				false,					// initially not owned
				MutexName.c_str());
		}
		else
		{
			_impl->hFileMutex = OpenMutexA(
				MUTEX_ALL_ACCESS,
				false,					// initially not owned
				MutexName.c_str());
		}

		SE_ASSERT(_impl->hFileMutex);
	}
	IPCMappedMemory::~IPCMappedMemory()
	{

	}

	bool IPCMappedMemory::IsValid() const
	{
		return (_impl && _impl->dataLink != nullptr);
	}


	size_t IPCMappedMemory::Size() const
	{
		return _memorySize;
	}

	uint8_t* IPCMappedMemory::Lock()
	{
		SE_ASSERT(_impl->hFileMutex);
		
		auto dwWaitResult = WaitForSingleObject(
			_impl->hFileMutex,    // handle to mutex
			INFINITE);  // no time-out interval
		
		return _impl->dataLink;
	}

	void IPCMappedMemory::Release()
	{
		SE_ASSERT(_impl->hFileMutex);		
		ReleaseMutex(_impl->hFileMutex);
	}


	void IPCMappedMemory::WriteMemory(const void* InMem, size_t DataSize, size_t Offset)
	{
		if (_impl->hFileMutex)
		{
			auto dwWaitResult = WaitForSingleObject(
				_impl->hFileMutex,    // handle to mutex
				INFINITE);  // no time-out interval

			memcpy(_impl->dataLink + Offset, InMem, DataSize);

			ReleaseMutex(_impl->hFileMutex);
		}
	}

	void IPCMappedMemory::ReadMemory(void* OutMem, size_t DataSize, size_t Offset)
	{
		if (_impl->hFileMutex)
		{
			auto dwWaitResult = WaitForSingleObject(
				_impl->hFileMutex,    // handle to mutex
				INFINITE);  // no time-out interval

			memcpy(OutMem, _impl->dataLink + Offset, DataSize);

			ReleaseMutex(_impl->hFileMutex);
		}
	}
}
#endif