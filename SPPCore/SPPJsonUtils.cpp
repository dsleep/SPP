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

	bool StringToJson(const std::string& InString, Json::Value &outValue)
	{
		Json::Value root;
		Json::CharReaderBuilder Builder;
		Json::CharReader* reader = Builder.newCharReader();
		std::string Errors;

		bool parsingSuccessful = reader->parse((char*)InString.data(), (char*)(InString.data() + InString.length()), &root, &Errors);
		delete reader;
		
		if (parsingSuccessful == false)
		{
			return false;
		}

		outValue = root;
		return true;
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