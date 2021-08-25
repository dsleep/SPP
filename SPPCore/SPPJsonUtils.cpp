// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPJsonUtils.h"
#include "SPPFileSystem.h"
#include "SPPLogging.h"
#include <string>

namespace SPP
{
	SPP_CORE_API LogEntry LOG_JSON("JSON");

	bool MemoryToJson(const void* InData, size_t DataSize, Json::Value& outValue)
	{
		Json::Value root;
		Json::CharReaderBuilder Builder;
		Json::CharReader* reader = Builder.newCharReader();
		std::string Errors;

		bool parsingSuccessful = reader->parse((const char*)InData, (const char*)InData + DataSize, &root, &Errors);
		delete reader;

		if (parsingSuccessful == false)
		{
			SPP_LOG(LOG_JSON, LOG_WARNING, "JSON ERROR: %s", Errors.c_str())
			return false;
		}

		outValue = root;
		return true;
	}

	bool StringToJson(const std::string& InString, Json::Value &outValue)
	{
		return MemoryToJson(InString.data(), InString.length(), outValue);
	}

	bool FileToJson(const char* FileName, Json::Value& outValue)
	{
		std::string FileString;
		if (LoadFileToString(FileName, FileString))
		{
			if (StringToJson(FileString, outValue))
			{
				return true;
			}
		}
		return false;
	}

}