// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPString.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"

#include <list>
#include <stdio.h>
#include <stdarg.h>
#include <memory>
#include <ostream>
#include <optional>
#include <fstream>
#include <sstream>

namespace SPP
{
	SPP_CORE_API LogEntry LOG_FILEIO("FILEIO");

	bool LoadFileToArray(const char* FileName, std::vector<uint8_t>& oFileData)
	{
		// load to end
		auto fileStream = std::make_unique<std::ifstream>(FileName, std::ifstream::in | std::ifstream::binary | std::ifstream::ate);

		if (fileStream->is_open())
		{
			auto fileSize = fileStream->tellg();
			// get size
			fileStream->seekg(0, std::ios::beg);				
			oFileData.resize(fileSize);
			fileStream->read((char*)oFileData.data(), fileSize);
			return true;
		}

		return false;
	}

	bool WriteArrayToFile(const char* FileName, const std::vector<uint8_t>& oFileData)
	{
		// load to end
		auto fileStream = std::make_unique<std::ofstream>(FileName, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);

		if (fileStream->is_open())
		{
			fileStream->write((char*)oFileData.data(), oFileData.size());
			return true;
		}

		return false;
	}

	bool LoadFileToString(const char* FileName, std::string& oFileString)
	{	
		auto fileStream = std::make_unique<std::ifstream>(FileName, std::ifstream::in);
		 
		if (fileStream->is_open())
		{
			std::stringstream strStream;
			strStream << fileStream->rdbuf();
			oFileString = strStream.str();
			return true;
		}

		return false;
	}

	bool WriteStringToFile(const char* FileName, const std::string& oFileString)
	{
		// load to end
		auto fileStream = std::make_unique<std::ofstream>(FileName, std::ofstream::out | std::ofstream::trunc);

		if (fileStream->is_open())
		{
			fileStream->write((char*)oFileString.data(), oFileString.size());
			return true;
		}

		return false;
	}

	std::vector<stdfs::path> GetAllFilesInDirectory(const char* InPath)
	{
		std::vector<stdfs::path> oPath;
		std::filesystem::path directoryPath(InPath);
		for (const auto& file : std::filesystem::directory_iterator(directoryPath)) {
			oPath.push_back(file.path());
		}
		return oPath;
	}

}