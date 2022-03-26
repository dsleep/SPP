// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPMemory.h"
#include "SPPLogging.h"
#include "SPPString.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstring>

namespace SPP
{
    LogEntry LOG_IPC("IPC");
    LogEntry LOG_MEM("MEM");
}

#if PLATFORM_MAC || PLATFORM_LINUX

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <semaphore.h>

namespace SPP
{
    struct IPCMappedMemory::PlatImpl
    {
        std::string storedName;
        int shm_fd = -1;
        uint8_t* dataLink = nullptr;
        sem_t *mutex_sem = nullptr;
    };

    IPCMappedMemory::IPCMappedMemory(const char* MappedName, size_t MemorySize, bool bIsNew) : _impl(new PlatImpl()), _memorySize(MemorySize)
    {
        SPP_LOG(LOG_IPC, LOG_INFO, "IPCMappedMemory::IPCMappedMemory: (%s:%d) %d", MappedName, bIsNew, MemorySize);
        
        int shmFlag = O_RDWR;
        if(bIsNew)
        {
            shmFlag |= O_CREAT;
        }
        
        _impl->storedName = MappedName;
        _impl->shm_fd = shm_open(MappedName, shmFlag, 0666);
        SE_ASSERT(_impl->shm_fd  != -1);
        
        std::string MutexName = std::string(MappedName) + "_M";
        
        _impl->mutex_sem = sem_open (MutexName.c_str(), bIsNew ? O_CREAT : 0, 0660, 0);
        SE_ASSERT(_impl->mutex_sem  != SEM_FAILED);
        
        if(bIsNew)
        {
            sem_post(_impl->mutex_sem);
            
            /* configure the size of the shared memory object */
            if(ftruncate(_impl->shm_fd, MemorySize) == -1)
            {
                SPP_LOG(LOG_IPC, LOG_INFO, "IPCMappedMemory::IPCMappedMemory: failed ftruncate");
            }
        }
         
        /* memory map the shared memory object */
        _impl->dataLink = (uint8_t*)mmap(0, MemorySize, PROT_READ | PROT_WRITE, MAP_SHARED, _impl->shm_fd, 0);
        SE_ASSERT(_impl->dataLink  != MAP_FAILED);

    }

    IPCMappedMemory::~IPCMappedMemory()
    {
        munmap( _impl->dataLink, _memorySize );
        /* remove the shared memory object */
        shm_unlink(_impl->storedName.c_str());
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
        sem_wait(_impl->mutex_sem);
        return _impl->dataLink;
    }

    void IPCMappedMemory::Release()
    {
        sem_post(_impl->mutex_sem);
    }


    void IPCMappedMemory::WriteMemory(const void* InMem, size_t DataSize, size_t Offset)
    {
        Lock();
        memcpy(_impl->dataLink + Offset, InMem, DataSize);
        Release();
    }

    void IPCMappedMemory::ReadMemory(void* OutMem, size_t DataSize, size_t Offset)
    {
        Lock();
        memcpy(OutMem, _impl->dataLink + Offset, DataSize);
        Release();
    }
}



#endif

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

namespace SPP
{
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
        //TODO FIX THIS!!!
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

namespace SPP
{ 
	const std::string& IPCDeadlockCheck::InitializeMonitor()
	{
		SE_ASSERT(!_IPCMem);
		_memoryID = std::generate_hex(3);
		_IPCMem.reset(new IPCMappedMemory(_memoryID.c_str(), 4, true));
		return _memoryID;
	}
	// return true if reporter updated
	bool IPCDeadlockCheck::CheckReporter()
	{
		uint32_t newTag = 0;
		_IPCMem->ReadMemory(&newTag, sizeof(newTag));
		if (newTag == _memTag)
		{
			return false;
		}
		else
		{
			_memTag = newTag;
			return true;
		}
	}

	//REPORTER
	void IPCDeadlockCheck::InitializeReporter(const std::string& InMemoryID)
	{
		SE_ASSERT(!_IPCMem);
		_memoryID = InMemoryID;
		_IPCMem.reset(new IPCMappedMemory(_memoryID.c_str(), 4, false));
	}
	void IPCDeadlockCheck::ReportToAppMonitor()
	{
		SE_ASSERT(_IPCMem);
		_memTag++;
		_IPCMem->WriteMemory(&_memTag, sizeof(_memTag));
	}
}
