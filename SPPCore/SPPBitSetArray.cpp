// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPBitSetArray.h"

namespace SPP
{
	BitSetArray::BitSetArray(size_t DesiredSize)
	{
		_numBytes = (DesiredSize + 7) / 8;
		bitData = (uint8_t*)malloc(_numBytes);
		SE_ASSERT(bitData);
		memset(bitData, 0, _numBytes);
	}

	void BitSetArray::Expand(size_t NewSize)
	{
		auto newByteSize = (NewSize + 7) / 8;
		if (newByteSize > _numBytes)
		{
			free(bitData);
			_numBytes = newByteSize;
			bitData = (uint8_t*)malloc(_numBytes);
			SE_ASSERT(bitData);
			memset(bitData, 0, _numBytes);
		}
	}

	void BitSetArray::Clear()
	{
		memset(bitData, 0, _numBytes);
	}

	void BitSetArray::Set(size_t Index, bool bValue)
	{
		size_t byteIndex = (Index >> 3);
		uint8_t bitIndex = 1 << (Index - (byteIndex << 3));

		if (bValue)
		{
			bitData[byteIndex] |= bitIndex;
		}
		else
		{
			bitData[byteIndex] &= ~bitIndex;
		}
	}

	bool BitSetArray::Get(size_t Index)
	{
		size_t byteIndex = (Index >> 3);
		uint8_t bitIndex = 1 << (Index - (byteIndex << 3));
		return (bitData[byteIndex] & bitIndex) != 0;		
	}

	BitReference BitSetArray::GetFirstFree()
	{
		for (size_t Iter = 0; Iter < _numBytes; Iter++)
		{
			if (bitData[Iter] != 0xFF)
			{
				for (uint8_t BitCheck = 0; BitCheck < 8; BitCheck++)
				{
					if ((bitData[Iter] & (1 << BitCheck)) == 0) 
					{
						return BitReference(&bitData[Iter], Iter * 8 + BitCheck);
					}
				}
			}
		}

		return BitReference(nullptr, 0);
	}

	BitSetArray::~BitSetArray()
	{
		free(bitData);
	}
		
	BitReference::BitReference(uint8_t* InByte, size_t InIdx) : thisByte(InByte), globalIdx(InIdx)
	{
		if (thisByte)
		{
			auto localIdx = static_cast<uint8_t>(globalIdx & 0x07);
			*thisByte |= (1 << localIdx);
		}
	}

	void BitReference::_release()
	{
		if (thisByte)
		{
			auto localIdx = static_cast<uint8_t>(globalIdx & 0x07);
			*thisByte &= ~(1 << localIdx);
			thisByte = nullptr;
			globalIdx = 0;
		}
	}

	BitReference::~BitReference()
	{
		_release();
	}

	BitReference::operator bool()
	{
		SE_ASSERT(thisByte);
		auto localIdx = static_cast<uint8_t>(globalIdx & 0x07);
		return (*thisByte) & (1 << localIdx);
	}
	bool BitReference::IsValid()
	{
		return thisByte != nullptr;
	}
}
