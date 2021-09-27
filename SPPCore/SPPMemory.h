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

	/*
	* A big old list of pool of elements and you just request a single element
	* to reduce footprints does some template uniqueness and static params
	*/
	template<typename T, typename U>
	class SingleRequestor
	{
	protected:
		uint64_t _totalSize = 0;
		// bit array
		std::vector<bool> _indices;
		T* _data = nullptr;

	public:
		class Reservation
		{
			friend class SingleRequestor;
			// using a U(unique) to avoid classes with multiple of same type
			inline static SingleRequestor<T, U>* Parent = nullptr;

		private:
			uint64_t Index = 0;

		public:
			Reservation(uint64_t InIndex)
			{
				Index = InIndex;
			}
			uint64_t GetIndex() const
			{
				return Index;
			}
			T* Get()
			{
				return Parent->GetAtIndex(Index);
			}
			~Reservation()
			{
				Parent->Free(this);
			}
		};

		SingleRequestor(uint64_t InSize)
		{
			SE_ASSERT(Reservation::Parent == nullptr);
			Reservation::Parent = this;
			_totalSize = InSize;
			_indices.resize(_totalSize, true);
			_data = new T[InSize];
		}

		~SingleRequestor()
		{
			SE_ASSERT(Reservation::Parent == this);
			Reservation::Parent = nullptr;
			delete[] _data;
		}

		std::unique_ptr<Reservation> Get()
		{
			for (uint32_t Iter = 0; Iter < _indices.size(); Iter++)
			{
				// could speed up with a uint8_t cast and check whole bytes, naive approach
				// or even a starting index for next go
				if (_indices[Iter])
				{
					_indices[Iter] = false;
					return std::make_unique< Reservation >(Iter);
				}
			}
			return nullptr;
		}

		T* GetAtIndex(uint64_t InIdx)
		{
			return _data[InIdx];
		}

		void Free(const Reservation *InReservation)
		{
			SE_ASSERT(Reservation::Parent == this);
			SE_ASSERT(_indices[InReservation->GetIndex()] == false);
			_indices[InReservation->GetIndex()] = true;
		}
	};

	/*
	* A big old pool but you request various sizes ideally of >1 proportion
	*/
	template<typename T, typename U>
	class BuddyAllocator
	{
	protected:
		uint64_t _totalSize = 0;
		uint64_t _minNodeSize = 0;
		T* _data = nullptr;

		// bit array
		std::vector<bool> _available;

	public:
		class Reservation
		{
			friend class BuddyAllocator;
			// using a U(unique) to avoid classes with multiple of same type
			inline static BuddyAllocator<T, U>* Parent = nullptr;

		private:
			uint64_t Index = 0;

		public:
			Reservation(uint64_t InIndex)
			{
				Index = InIndex;
			}
			uint64_t GetIndex() const
			{
				return Index;
			}
			T* Get()
			{
				return Parent->GetAtIndex(Index);
			}
			~Reservation()
			{
				Parent->Free(this);
			}
		};

		uint64_t roundUpToPow2(const uint64_t& InValue)
		{
			return (uint64_t)pow(2, ceil(log(InValue) / log(2)));
		}

		uint64_t powerOf2(const uint64_t& InValue)
		{
			return (uint64_t)ceil(log(InValue) / log(2));
		}

		BuddyAllocator(uint64_t InSize, uint64_t InMinSize)
		{
			SE_ASSERT(Reservation::Parent == nullptr);
			Reservation::Parent = this;

			_totalSize = roundUpToPow2(InSize);
			_minNodeSize = roundUpToPow2(InMinSize);			

			auto TotalNodeCnt = (_totalSize / _minNodeSize) << 1;
			_available.resize(TotalNodeCnt, true);
			_data = new T[_totalSize];
		}

		inline uint64_t TotalLevels(uint64_t InLevel)
		{
			return (powerOf2(_totalSize) - powerOf2(_minNodeSize));
		}

		inline uint64_t SizeAtLevel(uint64_t InLevel)
		{
			return (_totalSize >> InLevel);
		}

		inline uint64_t NodesAtLevel(uint64_t InLevel)
		{
			return (1 << InLevel);
		}

		inline uint64_t GetSiblingIdx(uint64_t InIdx)
		{
			return (InIdx & 0x01) ? (InIdx + 1) : (InIdx - 1);
		}

		inline uint64_t GetParentIdx(uint64_t InIdx)
		{
			return (InIdx - 1) >> 1;
		}

		inline uint64_t GetFirstChildIdx(uint64_t InIdx)
		{
			return (InIdx << 1) + 1;
		}

		Reservation *_GetData_Impl(uint64_t CurrentIdx, uint64_t InSize, int8_t Depth)
		{
			auto CurrentSize = SizeAtLevel(Depth);
			auto ChildSize = (CurrentSize >> 1);

			if (InSize > ChildSize)
			{
				if (InSize <= CurrentSize &&
					_available[CurrentIdx])
				{
					_available[CurrentIdx] = false;
					return new Reservation(CurrentIdx);
				}
			}
			else
			{
				auto ChildLeft = GetFirstChildIdx(CurrentIdx);
				auto ChildRight = ChildLeft + 1;

				if (auto didAllocate = _GetData_Impl(ChildLeft, InSize, Depth+1))
				{
					_available[CurrentIdx] = false;
					return didAllocate;
				}
				if (auto didAllocate = _GetData_Impl(ChildRight, InSize, Depth+1))
				{
					_available[CurrentIdx] = false;
					return didAllocate;
				}
			}

			return nullptr;
		}

		std::unique_ptr<Reservation> Get(uint64_t InSize)
		{
			return std::unique_ptr< Reservation>(_GetData_Impl(0, InSize, 0));
		}

		void FreeIdx(uint64_t InIdx)
		{
			SE_ASSERT(_available[InIdx] == false);
			_available[InIdx] = true;

			//stop at top
			if (InIdx)
			{
				auto siblingIdx = GetSiblingIdx(InIdx);
				if (_available[siblingIdx])
				{
					auto parentIdx = GetParentIdx(InIdx);
					FreeIdx(parentIdx);
				}
			}
		}

		void Free(const Reservation* InReservation)
		{
			SE_ASSERT(Reservation::Parent == this);
			FreeIdx(InReservation->GetIndex());
		}
	};
}