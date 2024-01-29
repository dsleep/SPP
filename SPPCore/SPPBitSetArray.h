// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"

namespace SPP
{
	class SPP_CORE_API BitReference
	{
		NO_COPY_ALLOWED(BitReference);

	private:
		size_t globalIdx = 0;
		uint8_t* thisByte = nullptr;

		void _release();
	public:
		BitReference() {}
		BitReference(uint8_t* InByte, size_t InIdx);
		BitReference(BitReference&& InBitSet) noexcept
		{
			std::swap(thisByte, InBitSet.thisByte);
			std::swap(globalIdx, InBitSet.globalIdx);
		}
		BitReference &operator=(BitReference&& InBitSet) noexcept
		{
			_release();
			std::swap(thisByte, InBitSet.thisByte);
			std::swap(globalIdx, InBitSet.globalIdx);
			return *this;
		}
		~BitReference();
		operator bool();
		bool IsValid();
		size_t Index() const
		{
			return globalIdx;
		}
	};

	template <int A, int B>
	struct get_power
	{
		static const int value = A * get_power<A, B - 1>::value;
	};
	template <int A>
	struct get_power<A, 0>
	{
		static const int value = 1;
	};

	template <int A>
	struct get_power_of_two
	{
		static const int value = 1 + get_power_of_two<A / 2>::value;
	};
	template <>
	struct get_power_of_two<1>
	{
		static const int value = 0;
	};
	template <>
	struct get_power_of_two<0>
	{
		static const int value = 0;
	};

	template<typename StorageType = uint32_t>
	class BitSetArray
	{
		NO_COPY_ALLOWED(BitSetArray);

	private:

		size_t _numBits = 0;
		size_t _numBytes = 0;
		size_t _arrayCount = 0;
				
		StorageType* _data = nullptr;

		static constexpr size_t const StorageTypeBitCount = sizeof(StorageType) * 8;
		static constexpr size_t const StorageAllBits = ~StorageType(0);
		static constexpr size_t const StorageBitCountPow2 = get_power_of_two< StorageTypeBitCount >::value;

	public:
		BitSetArray() {}
		BitSetArray(size_t InBitCount)
		{
			Initialize(InBitCount);
		}

		void Initialize(size_t InBitCount)
		{
			FreeData();

			_numBits = InBitCount;
			_arrayCount = ((InBitCount + StorageTypeBitCount - 1) >> 3) / sizeof(StorageType);
			_numBytes = _arrayCount * sizeof(StorageType);

			SE_ASSERT((_numBytes * 8) >= _numBits);
			_data = (StorageType*)malloc(_numBytes);
			SE_ASSERT(_data);
			ClearBits();
		}

		auto GetNumBytes() const
		{
			return _numBytes;
		}

		auto GetNumBits() const
		{
			return _numBits;
		}

		auto GetArray() const
		{
			return _data;
		}

		void ClearBits()
		{
			memset(_data, 0, _numBytes);
		}

		void FreeData()
		{
			if (_data)
			{
				free(_data);
				_data = nullptr;
			}
		}

		void Expand(size_t NewBitSize)
		{
			if (NewBitSize > _numBits)
			{
				Initialize(NewBitSize);
			}
		}

		void Set(size_t Index, bool bValue)
		{
			SE_ASSERT(Index < _numBits);

			StorageType bitMask = 1 << (Index & (StorageTypeBitCount - 1));
			size_t storageIndex = Index >> StorageBitCountPow2;

			SE_ASSERT(storageIndex < _arrayCount);
			if (bValue)
			{
				_data[storageIndex] |= bitMask;
			}
			else
			{
				_data[storageIndex] &= ~bitMask;
			}
		}

		bool Get(size_t Index)
		{
			SE_ASSERT(Index < _numBits);
			StorageType bitMask = 1 << (Index & (StorageTypeBitCount - 1));
			StorageType storageIndex = Index >> StorageBitCountPow2;

			SE_ASSERT(storageIndex < _arrayCount);
			return (_data[storageIndex] & bitMask) != 0;
		}
		
		BitReference GetFirstFree()
		{
			for (size_t Iter = 0; Iter < _numBits; Iter++)
			{
				if (!Get(Iter))
				{
					return BitReference((uint8_t*)_data + (Iter >> 3), Iter);
				}
			}

			return BitReference(nullptr, 0);
		}

		~BitSetArray()
		{
			FreeData();
		}
	};


	template<typename IndexedData>
	class LeaseManager
	{
	protected:
		IndexedData& _leasor;
		std::unique_ptr< BitSetArray<uint32_t> > _bitArray;

	public:
		class Reservation 
		{
			NO_COPY_ALLOWED(Reservation);

		private:
			BitReference _bitRef;
			LeaseManager* _owner = nullptr;
			IndexedData::value_type& _data;

		public:
			struct LeaseWrite
			{
				NO_COPY_ALLOWED(LeaseWrite);

				IndexedData::value_type& data;
				Reservation& lease;

				LeaseWrite(IndexedData::value_type& InData, Reservation& InLease) : data(InData), lease(InLease)
				{

				}

				~LeaseWrite()
				{
					lease.Update();
				}
			};

			Reservation(LeaseManager* InOwner, 
				IndexedData::value_type& InData, 
				BitReference &&InBitRef) : _owner(InOwner), _data(InData), _bitRef(std::move(InBitRef))
			{
			}
			~Reservation()
			{
				_owner->EndLease(*this);
			}
			LeaseWrite Access()
			{
				return LeaseWrite(_data,*this);
			}
			BitReference& GetBitReference()
			{
				return _bitRef;
			}
			void Update()
			{
				_owner->LeaseUpdated(*this);
			}
			auto GetIndex()
			{
				return _bitRef.Index();
			}
		};

		LeaseManager(IndexedData& InIndexor) : _leasor(InIndexor) 
		{
			_bitArray = std::make_unique< BitSetArray<uint32_t> >(_leasor.size());
		}

		std::shared_ptr<Reservation> GetLease()
		{
			BitReference freeElement = _bitArray->GetFirstFree();
			if (!freeElement.IsValid())
			{
				return nullptr;
			}
			auto& leaseData = _leasor[freeElement.Index()];
			return std::make_shared< Reservation >(this, leaseData, std::move(freeElement));
		}

		virtual void LeaseUpdated(Reservation& InLease)
		{

		}

		virtual void EndLease(Reservation& InLease)
		{

		}
	};
}