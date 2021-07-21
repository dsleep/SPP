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
	SPP_CORE_API bool LoadFileToArray(const char* FileName, std::vector<uint8_t>& oFileData);
	SPP_CORE_API bool WriteArrayToFile(const char* FileName, const std::vector<uint8_t>& oFileData);
		
	SPP_CORE_API bool LoadFileToString(const char* FileName, std::string& oFileString);
	//TODO
	//SPP_CORE_API bool WriteStringToFile(const char* FileName, const std::string& oFileString);
}