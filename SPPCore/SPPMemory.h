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
		size_t _memorySize;

	public:		
		IPCMappedMemory(const char* MappedName, size_t MemorySize, bool bIsNew);
		~IPCMappedMemory();

		size_t Size() const;
		uint8_t* Lock();
		void Release();

		bool IsValid() const;
		void WriteMemory(const void* InMem, size_t DataSize, size_t Offset = 0);
		void ReadMemory(void* OutMem, size_t DataSize, size_t Offset = 0);
	};

	template<typename T>
	class SimpleIPCMessageQueue
	{
	private:
		IPCMappedMemory& memLink;
		size_t _queueOffset = 0;

	public:
		SimpleIPCMessageQueue(IPCMappedMemory &InMem, size_t InOffset = 0) : memLink(InMem), _queueOffset(InOffset)
		{
			static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value);
		}

		// Push a new Message
		void PushMessage(const T &InMessage)
		{
			auto DataPtr = memLink.Lock() + _queueOffset;

			uint32_t ElementCount = *(uint32_t*)DataPtr;
			auto WritePtr = DataPtr + sizeof(uint32_t);
			uint32_t DataOffset = ElementCount * sizeof(T);
			
			if ((DataOffset + sizeof(uint32_t) + sizeof(T) + _queueOffset) <= memLink.Size())
			{
				memcpy(WritePtr + DataOffset, &InMessage, sizeof(T));
				*(uint32_t*)DataPtr = ElementCount + 1; //add a message count
			}

			memLink.Release();
		}

		// Gets and Clears all messages
		std::vector<T> GetMessages()
		{
			std::vector<T> oMessages;

			auto DataPtr = memLink.Lock() + _queueOffset;

			uint32_t ElementCount = *(uint32_t*)DataPtr;
			auto WritePtr = DataPtr + sizeof(uint32_t);

			if (ElementCount > 0)
			{
				oMessages.resize(ElementCount);
				memcpy(oMessages.data(), WritePtr, sizeof(T) * ElementCount);
				*(uint32_t*)DataPtr = 0; 
			}

			memLink.Release();

			return oMessages;
		}
	};
}