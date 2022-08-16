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

	class SPP_CORE_API BitSetArray
	{
		NO_COPY_ALLOWED(BitSetArray);

	private:
		size_t _numBytes = 0;
		uint8_t* bitData = nullptr;

	public:
		BitSetArray(size_t BitCount);	
		
		BitReference GetFirstFree();

		~BitSetArray();
	};


	template<typename IndexedData>
	class LeaseManager
	{
	protected:
		IndexedData& _leasor;
		std::unique_ptr<BitSetArray> _bitArray;

	public:
		class Lease 
		{
			NO_COPY_ALLOWED(Lease);

		private:
			BitReference _bitRef;
			LeaseManager* _owner = nullptr;
			IndexedData::value_type& _data;

		public:
			struct LeaseWrite
			{
				NO_COPY_ALLOWED(LeaseWrite);

				IndexedData::value_type& data;
				Lease& lease;

				LeaseWrite(IndexedData::value_type& InData, Lease& InLease) : data(InData), lease(InLease)
				{

				}

				~LeaseWrite()
				{
					lease.Update();
				}
			};

			Lease(LeaseManager* InOwner, 
				IndexedData::value_type& InData, 
				BitReference &&InBitRef) : _owner(InOwner), _data(InData), _bitRef(std::move(InBitRef))
			{
			}
			~Lease()
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
			_bitArray = std::make_unique< BitSetArray >(_leasor.size());
		}

		std::shared_ptr<Lease> GetLease()
		{
			BitReference freeElement = _bitArray->GetFirstFree();
			if (!freeElement.IsValid())
			{
				return nullptr;
			}
			auto& leaseData = _leasor[freeElement.Index()];
			return std::make_shared< Lease >(this, leaseData, std::move(freeElement));
		}

		virtual void LeaseUpdated(Lease& InLease)
		{

		}

		virtual void EndLease(Lease& InLease)
		{

		}
	};
}