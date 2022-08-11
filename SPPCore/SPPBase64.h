// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

// closest to my style taken from DaedalusAlpha https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c


#pragma once

#include "SPPCore.h"
#include <vector>
#include <string>

namespace SPP
{
	SPP_CORE_API std::string base64_encode(uint8_t const* buf, size_t bufLen);
	SPP_CORE_API std::vector<uint8_t> base64_decode(std::string const&);
}