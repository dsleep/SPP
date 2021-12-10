// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGUID.h"
#include <random>
#include <sstream>

namespace SPP
{
	GUID GUID::Create()
	{
		static_assert(sizeof(GUID) == 16);
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<uint32_t> dis;				
		return GUID(dis(gen), dis(gen), dis(gen), dis(gen));
	}

	//todo speed up
	GUID::GUID(const char* InString)
	{
		auto stringSize = ::strlen(InString);
		if (stringSize == (8 * 4))
		{	
			for(uint8_t Iter = 0; Iter<4; Iter++)
			{
				std::string localString(InString, 8);
				std::stringstream ss;
				ss << std::hex << localString;
				ss >> (*this)[Iter];
				InString += 8;
			}
		}
	}

	uint32_t& GUID::operator[](size_t Index)
	{
		switch(Index)
		{
		case 0:
			return A;
		case 1:
			return B;
		case 2:
			return C;
		case 3:
			return D;
		}

		// shouldn't get here
		SE_ASSERT(false);
		return A;
	}

	template <typename I>
	void Create32BitHex(char *dst, I w, size_t hex_len = sizeof(I) << 1)
	{
		static_assert(sizeof(I) == 4);
		static const char* digits = "0123456789ABCDEF";		
		for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
		{
			dst[i] = digits[(w >> j) & 0x0f];
		}
	}

	std::string GUID::ToString() const
	{
		std::string outHex(8 * 4, '0');
		Create32BitHex(outHex.data(), A);
		Create32BitHex(outHex.data() + 8, B);
		Create32BitHex(outHex.data() + 16, C);
		Create32BitHex(outHex.data() + 24, D);
		return outHex;
	}
}