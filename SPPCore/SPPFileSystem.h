// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
	
#ifdef _WIN32
	#include <filesystem>
	namespace stdfs = std::filesystem;
#elif defined(__linux__)
	#if __GNUC__ > 7
		#include <filesystem>
		namespace stdfs = std::filesystem;
	#else
		#include <experimental/filesystem>	
		namespace stdfs = std::experimental::filesystem;
	#endif
#elif __APPLE__
    #include <filesystem>
    namespace stdfs = std::filesystem;
#else
	#error "Unsupported platform"
#endif

namespace SPP
{
	SPP_CORE_API bool LoadFileToArray(const char* FileName, std::vector<uint8_t>& oFileData);
	SPP_CORE_API bool WriteArrayToFile(const char* FileName, const std::vector<uint8_t>& oFileData);
		
	SPP_CORE_API bool LoadFileToString(const char* FileName, std::string& oFileString);
	SPP_CORE_API bool WriteStringToFile(const char* FileName, const std::string& oFileString);
}
