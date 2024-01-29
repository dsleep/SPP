// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPBitSetArray.h"

namespace SPP
{
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
