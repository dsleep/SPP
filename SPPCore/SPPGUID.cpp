// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGUID.h"
#include <random>

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
}