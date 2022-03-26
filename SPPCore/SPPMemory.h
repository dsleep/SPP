// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPLogging.h"

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <math.h>

namespace SPP
{
	extern SPP_CORE_API LogEntry LOG_MEM;

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

	class SPP_CORE_API IPCDeadlockCheck
	{
	private:
		std::string _memoryID;
		std::unique_ptr< IPCMappedMemory> _IPCMem;
		uint32_t _memTag = 123;

	public:
		//MONITOR
		const std::string& InitializeMonitor();
		// return true if reporter updated
		bool CheckReporter();
		//REPORTER
		void InitializeReporter(const std::string& InMemoryID);
		void ReportToAppMonitor();
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

	template<typename T>
	class PlainOldDataStorage
	{
	private:
		T* _data = nullptr;
	public:
		PlainOldDataStorage() = default;
		~PlainOldDataStorage()
		{
			Free();
		}
		void Allocate(size_t InCount)
		{
			SE_ASSERT(_data == nullptr);
			_data = (T*)malloc(sizeof(T) * InCount);
		}
		void Free()
		{
			if (_data)
			{
				free(_data);
				_data = nullptr;
			}
		}
		T* operator[](size_t Index)
		{
			return _data + Index;
		}
	};

	/*
	* A big old list of pool of elements and you just request a single element
	* to reduce footprints does some template uniqueness and static params
	*/
	template<typename T, typename Storage = PlainOldDataStorage<T>, typename U=char>
	class SingleRequestor
	{
	protected:
		uint16_t _totalCount = 0;
		std::vector<uint16_t> _available;
		Storage _storage;

	public:
		class Reservation
		{
			friend class SingleRequestor;
			// using a U(unique) to avoid classes with multiple of same type
			inline static SingleRequestor<T, Storage, U>* Parent = nullptr;

		private:
			uint16_t Index = 0;

		public:
			Reservation(uint16_t InIndex)
			{
				Index = InIndex;
			}
			uint16_t GetIndex() const
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

		SingleRequestor(uint64_t InCount)
		{
			SE_ASSERT(Reservation::Parent == nullptr);
			Reservation::Parent = this;
			_totalCount = InCount;
			
			_available.resize(_totalCount);
			for (size_t Iter = 0; Iter < _available.size(); Iter++)
			{
				_available[_available.size() - Iter - 1] = Iter;
			}

			_storage.Allocate(_totalCount);
		}

		~SingleRequestor()
		{
			SE_ASSERT(Reservation::Parent == this);
			SE_ASSERT(_available.size() == _totalCount);
			Reservation::Parent = nullptr;
			_storage.Free();
			_available.clear();
		}

		std::unique_ptr<Reservation> Get()
		{
			SE_ASSERT(_available.empty() == false);

			auto backIndex = _available.back();
			_available.pop_back();

			return std::make_unique< Reservation >(backIndex);
		}

		T* GetAtIndex(uint16_t InIdx)
		{
			return _storage[InIdx];
		}

		void Free(const Reservation *InReservation)
		{
			SE_ASSERT(Reservation::Parent == this);
			auto resIdx = InReservation->GetIndex();
			_available.push_back(resIdx);
		}
	};

	
	/*
	* A big old pool but you request various sizes ideally of >1 proportion
	*/
	template<typename T, typename Storage = PlainOldDataStorage<T>, typename U = char>
	class BuddyAllocator
	{
	protected:
		uint64_t _totalSize = 0;
		uint64_t _minNodeSize = 0;
		uint64_t _reservedAmount = 0;
		uint64_t _wasteAmount = 0;
		Storage _storage;

		// bit array
		std::vector<bool> _available;
		std::vector<bool> _reserved;

	public:
		class Reservation
		{
			friend class BuddyAllocator;
			// using a U(unique) to avoid classes with multiple of same type
			inline static BuddyAllocator<T, Storage, U>* Parent = nullptr;

		private:
			uint64_t Index = 0;
			uint64_t _size = 0;

		public:
			Reservation(uint64_t InIndex, uint64_t InSize)
			{
				Index = InIndex;
				_size = InSize;
			}
			uint64_t GetIndex() const
			{
				return Index;
			}
			uint64_t GetSize() const
			{
				return _size;
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

		uint64_t roundDownToPow2(const uint64_t& InValue)
		{
			return (uint64_t)pow(2, floor(log(InValue) / log(2)));
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
			_reserved.resize(TotalNodeCnt, false);
			_storage.Allocate(_totalSize);
		}

		~BuddyAllocator()
		{
			SE_ASSERT(Reservation::Parent == this);
			Reservation::Parent = nullptr;
			_storage.Free();
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

			if (InSize > ChildSize || CurrentSize <= _minNodeSize)
			{
				if (InSize <= CurrentSize && _available[CurrentIdx])
				{
					SPP_LOG(LOG_MEM, LOG_VERBOSE, "Reserve: %d", CurrentIdx);
					_available[CurrentIdx] = false;
					_reserved[CurrentIdx] = true;
					_reservedAmount += CurrentSize;
					_wasteAmount += (CurrentSize - InSize);
					return new Reservation(CurrentIdx, InSize);
				}
			}
			else if(_reserved[CurrentIdx] == false)
			{
				auto ChildLeft = GetFirstChildIdx(CurrentIdx);
				auto ChildRight = ChildLeft + 1;

				if (auto didAllocate = _GetData_Impl(ChildLeft, InSize, Depth+1))
				{
					SPP_LOG(LOG_MEM, LOG_VERBOSE, "Mark Partial Reserve: %d", CurrentIdx);
					_available[CurrentIdx] = false;
					return didAllocate;
				}
				if (auto didAllocate = _GetData_Impl(ChildRight, InSize, Depth+1))
				{
					SPP_LOG(LOG_MEM, LOG_VERBOSE, "Mark Partial Reserve: %d", CurrentIdx);
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

		T* GetAtIndex(uint64_t InIdx)
		{
			return _storage[InIdx];
		}

		void FreeIdx(uint64_t InIdx)
		{
			SPP_LOG(LOG_MEM, LOG_VERBOSE, "Free Partial: %d", InIdx);

			SE_ASSERT(_reserved[InIdx] == false);
			SE_ASSERT(_available[InIdx] == false);
			_available[InIdx] = true;

			//stop at top
			if (InIdx)
			{
				auto siblingIdx = GetSiblingIdx(InIdx);
				// if sibling available, that parent is now available
				if (_available[siblingIdx])
				{
					auto parentIdx = GetParentIdx(InIdx);
					FreeIdx(parentIdx);
				}
			}
		}

		void Free(const Reservation* InReservation)
		{
			auto ReserverIdx = InReservation->GetIndex();
			SE_ASSERT(Reservation::Parent == this);
			SE_ASSERT(_reserved[ReserverIdx]);
			_reserved[ReserverIdx] = false;
			
			auto curRowIdx = roundDownToPow2(ReserverIdx + 1);
			auto Depth = powerOf2(curRowIdx);
			auto CurrentSize = SizeAtLevel(Depth);
			_reservedAmount -= CurrentSize;
			_wasteAmount -= (CurrentSize - InReservation->GetSize());

			SPP_LOG(LOG_MEM, LOG_VERBOSE, "Free Reserved: %d", ReserverIdx);

			FreeIdx(ReserverIdx);
		}

		void Report()
		{
			size_t TotalReservations = 0;
			for (size_t Iter = 0; Iter < _reserved.size(); Iter++)
			{
				if (_reserved[Iter])
				{
					TotalReservations++;
				}
			}
			SPP_LOG(LOG_MEM, LOG_INFO, "TotalReservations: %d Size: %d Waste: %d", 
				TotalReservations, _reservedAmount, _wasteAmount);
		}
	};

	template<typename T, typename Storage = PlainOldDataStorage<T>, typename U = char>
	class LinkedListLinearFitAllocator
	{
	protected:
		struct DataNode
		{
			inline static uint32_t NodeCount = 0;

			uint64_t Start = 0;
			uint64_t Size = 0;
			bool bClaimed = false;

			DataNode(uint64_t InStart, uint64_t InSize) : Start(InStart), Size(InSize)
			{
				NodeCount++;
			}

			~DataNode()
			{
				NodeCount--;
			}

			void Die()
			{
				SE_ASSERT(bClaimed == false);

				if (next)
				{
					next->prev = prev;
				}

				if (prev)
				{
					prev->next = next;
				}
				 
				next = nullptr;
				prev = nullptr;
				delete this;
			}

			void EatNext()
			{
				SE_ASSERT(bClaimed == false);
				SE_ASSERT(next != nullptr);

				Size += next->Size;
				next->Die();
			}

			DataNode* Fit(uint64_t InSize)
			{
				if (bClaimed == false && InSize <= Size)
				{
					auto remainder = (Size - InSize);

					if (remainder)
					{
						auto NewNode = new DataNode( Start + InSize, remainder);

						if (next)
						{
							next->prev = NewNode;
							NewNode->next = next;
						}

						Size = InSize;
						next = NewNode;
						NewNode->prev = this;
						
						// we are now claimed
						bClaimed = true;
						return this;
					}
					// if nothing left we just claim and return
					else
					{
						bClaimed = true;
						return this;
					}
				}

				return nullptr;
			}

			DataNode* next = nullptr;
			DataNode* prev = nullptr;
		};

		uint64_t _totalSize = 0;
		Storage _storage;
		DataNode* _rootNode = nullptr;

	public:
		class Reservation
		{
			friend class LinkedListLinearFitAllocator;
			// using a U(unique) to avoid classes with multiple of same type
			inline static LinkedListLinearFitAllocator<T, Storage, U>* Parent = nullptr;

		private:
			DataNode *_nodeLink = 0;

		public:
			Reservation(DataNode* InLink)
			{
				_nodeLink = InLink;
			}
			uint64_t GetSize() const
			{
				return _nodeLink->Size;
			}
			DataNode* GetDataNode() const
			{
				return _nodeLink;
			}
			T* Get()
			{
				return Parent->GetData(_nodeLink);
			}
			~Reservation()
			{
				Parent->Free(this);
			}
		};

		LinkedListLinearFitAllocator(uint64_t InSize)
		{
			SE_ASSERT(Reservation::Parent == nullptr);
			Reservation::Parent = this;

			_totalSize = InSize;
			_storage.Allocate(_totalSize);

			_rootNode = new DataNode(0, _totalSize);
		}

		~LinkedListLinearFitAllocator()
		{
			SE_ASSERT(Reservation::Parent == this);
			Reservation::Parent = nullptr;
			_storage.Free();
		}

		Reservation* _GetData_Impl(uint64_t InSize)
		{
			SE_ASSERT(_rootNode);

			auto curNode = _rootNode;
			while (curNode)
			{
				if (auto foundFit = curNode->Fit(InSize))
				{
					return new Reservation(foundFit);
				}
				curNode = curNode->next;
			}

			return nullptr;
		}

		std::unique_ptr<Reservation> Get(uint64_t InSize)
		{
			return std::unique_ptr< Reservation>(_GetData_Impl( InSize ));
		}

		T* GetData(DataNode* InNode)
		{
			return _storage[InNode->Start];
		}

		void CheckCombine(DataNode* InNode)
		{
			SE_ASSERT(InNode->bClaimed == false);

			// only eat next so root node never needs to be checked

			if (InNode->next && InNode->next->bClaimed == false)
			{
				InNode->EatNext();
				CheckCombine(InNode);
				return;
			}

			if (InNode->prev && InNode->prev->bClaimed == false)
			{
				auto prevNode = InNode->prev;
				prevNode->EatNext();
				CheckCombine(prevNode);
				return;
			}
		}
		
		void Free(Reservation* InReservation)
		{
			SE_ASSERT(Reservation::Parent == this);

			auto freedNode = InReservation->_nodeLink;
			SE_ASSERT(freedNode->bClaimed);

			freedNode->bClaimed = false;

			CheckCombine(freedNode);

			InReservation->_nodeLink = nullptr;
		}

		void Report()
		{
			uint32_t NodeCount = 0;
			size_t Reserved = 0;
			size_t Available = 0;
			auto curNode = _rootNode;
			while (curNode)
			{
				if (curNode->bClaimed) Reserved += curNode->Size;
				else Available += curNode->Size;

				NodeCount++;
				curNode = curNode->next;
			}

			SPP_LOG(LOG_MEM, LOG_INFO, "TotalReservations: Nodes %d Reserved: %d Available: %d Total: %d",
				NodeCount, Reserved, Available, Available + Reserved);
			SE_ASSERT(DataNode::NodeCount == NodeCount);
		}
	};
}
