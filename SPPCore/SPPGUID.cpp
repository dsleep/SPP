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

	GUID::GUID(const char* InString)
	{
		auto stringSize = std::strlen(InString);
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

	std::string GUID::ToString() const
	{
		std::stringstream stream;
		stream << std::hex << A;
		stream << std::hex << B;
		stream << std::hex << C;
		stream << std::hex << D;
		return stream.str();
	}
}