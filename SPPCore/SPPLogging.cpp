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

#if _WIN32
	#include "Windows.h"
#endif


namespace SPP
{
	SPP_CORE_API LogEntry LOG_SPP_GENERIC("LOG");

	SPP_CORE_API SLogManager & INSTANCE()
	{
		static SLogManager sO;
		return sO;
	}
	
	std::list<LogEntry*> &GetEntryList()
	{
		static std::list<LogEntry*> sO;
		return sO;
	}

	LogEntry::LogEntry(const char *InLogName, bool bInEnable, uint8_t InMinLogLevel) : LogName(InLogName), bEnabled(bInEnable), MinLogLevel(InMinLogLevel)
	{
		GetEntryList().push_back(this);
	}

	LogEntry::~LogEntry()
	{
		GetEntryList().remove(this);
	}	

	SPP_CORE_API void ReportLogEntries(std::ostream& os)
	{
		auto &entryList = GetEntryList();

		for (auto entry : entryList)
		{
			os << "ENTRY: " << entry->LogName << std::endl;
			os << " - bEnabled " << entry->bEnabled << std::endl;
			os << " - MinLogLevel: " << entry->MinLogLevel << std::endl;
		}
	}

	SPP_CORE_API void LoggingMacro(const std::string &InString)
	{
		auto commandSplit = str_split(InString, ' ');

		if (commandSplit.size() == 2)
		{
			std::optional<bool> bIsShowing;
			std::string whom;

			if (commandSplit.front() == "hide")
			{
				bIsShowing = false;
			}
			else if (commandSplit.front() == "unhide")
			{
				bIsShowing = true;
			}
			
			if (bIsShowing.has_value())
			{
				whom = commandSplit.back();
				bool bIsAll = (whom == "all");
				auto &entryList = GetEntryList();
				for (auto entry : entryList)
				{
					if (bIsAll || str_equals(whom, entry->LogName))
					{
						entry->bEnabled = bIsShowing.value();
					}
				}
			}
		}
	}

#define LINEFILEBUFFERSIZE 100
#define LOGBUFFERSIZE 1024

	void ConsoleLog::Log(const LogEntry &LogEntry, int line, const char* file, const char* format, ...)
	{
		char finalBuffer[LOGBUFFERSIZE + LINEFILEBUFFERSIZE];
		snprintf(finalBuffer, LINEFILEBUFFERSIZE, "(%s)%ls:%d:: ", LogEntry.LogName, stdfs::path(file).filename().c_str(), line);

		char buffer[LOGBUFFERSIZE];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, LOGBUFFERSIZE, format, args);
		va_end(args);

		strncat(finalBuffer, buffer, LOGBUFFERSIZE);

		// LOG SPECIAL?
		printf("%s\n", finalBuffer);

#if _WIN32
		OutputDebugStringA(finalBuffer);
		OutputDebugStringA("\n");
#endif
	}

	// doesn't buffer
	void ConsoleLog::Flush()
	{
	}


	FileLogger::FileLogger(const char *FileName)
	{
		_fileStream = std::make_unique<std::ofstream>(FileName, std::ifstream::out | std::ifstream::binary);
	}
	FileLogger::~FileLogger()
	{
		_fileStream->close();
	}

	void FileLogger::Log(const LogEntry &LogEntry, int line, const char* file, const char* format, ...)
	{
		char finalBuffer[LOGBUFFERSIZE + LINEFILEBUFFERSIZE];
		snprintf(finalBuffer, LINEFILEBUFFERSIZE, "(%s)%ls:%d:: ", LogEntry.LogName, stdfs::path(file).filename().c_str(), line);

		char buffer[LOGBUFFERSIZE];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, LOGBUFFERSIZE, format, args);
		va_end(args);

		strncat(finalBuffer, buffer, LOGBUFFERSIZE);

		// LOG SPECIAL?
		(*_fileStream) << finalBuffer;
		(*_fileStream) << std::endl;
	}

	void FileLogger::Flush()
	{

	}


}