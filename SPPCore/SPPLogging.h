// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"

#include <algorithm>
#include <string>
#include <memory>
#include <vector>
#include <list>

namespace SPP
{
	struct SPP_CORE_API LogEntry
	{
		bool bEnabled = true;
		const char *LogName;
		uint8_t MinLogLevel = 0xFF;

		LogEntry(const char *InLogName, bool bInEnable = true, uint8_t InMinLogLevel = 0xFF);
		virtual ~LogEntry();
	};

	SPP_CORE_API extern LogEntry LOG_SPP_GENERIC;

	class SPP_CORE_API ILogInterface
	{
	public:
		virtual void Log(const LogEntry &LogEntry, int line, const char* file, const char* format, ...) = 0;
		virtual void Flush() = 0;
		virtual ~ILogInterface() {};
	};

	class SPP_CORE_API ConsoleLog : public ILogInterface
	{
	public:
		virtual void Log(const LogEntry &LogEntry, int line, const char* file, const char* format, ...);
		virtual void Flush();
		virtual ~ConsoleLog() { };
	};

	class SPP_CORE_API FileLogger : public ILogInterface
	{
	private:
		std::unique_ptr<std::ofstream> _fileStream;

	public:
		FileLogger(const char *FileName);
		virtual ~FileLogger();

		virtual void Log(const LogEntry &LogEntry, int line, const char* file, const char* format, ...);
		virtual void Flush();		
	};

	class SPP_CORE_API SLogManager
	{
		friend SPP_CORE_API SLogManager &INSTANCE();

		// delete copy and move constructors and assign operators
		NO_COPY_ALLOWED(SLogManager);
		NO_MOVE_ALLOWED(SLogManager);

	private:
		uint8_t _logLevel = 0xFF;

		SLogManager() {}
		~SLogManager() {}
						
		std::list< std::unique_ptr< ILogInterface > > Loggers;

	public:					
		template<typename T, typename... Args>
		void Attach(Args... args)
		{
			Loggers.push_back(std::make_unique<T>(args...));
		}

		void Dettach(const ILogInterface* Logger)
		{
			for (auto iter = Loggers.begin(); iter != Loggers.end(); )
			{
				if ((*iter).get() == Logger)
				{
					iter = Loggers.erase(iter); // _advances_ iter, so this loop is not infinite
				}
				else
				{
					++iter;
				}
			}
		}

		void SetLogLevel(uint8_t InLogLevel)
		{
			_logLevel = InLogLevel;
		}

		// variadic template passing log
		template<typename... Args>
		void Log(const LogEntry &LogEntry, uint8_t InLogLevel, int line, const char* file, const char* format, Args... args)
		{
			uint8_t MinnedLogLevel = std::min<uint8_t>(InLogLevel, LogEntry.MinLogLevel);
			if (LogEntry.bEnabled == false || MinnedLogLevel > _logLevel )
			{
				return;
			}

			for (auto &Logger : Loggers)
			{
				Logger->Log(LogEntry, line, file, format, args...);
			}
		}

		void Flush()
		{
			for (auto &Logger : Loggers)
			{
				Logger->Flush();
			}
		}
	};

	SPP_CORE_API SLogManager & INSTANCE();

	SPP_CORE_API void ReportLogEntries(std::ostream& os);
	SPP_CORE_API void LoggingMacro(const std::string &InString);
		
	const uint8_t LOG_ERROR = 0;
	const uint8_t LOG_WARNING = 1;
	const uint8_t LOG_INFO = 2;
	const uint8_t LOG_VERBOSE = 3;
}

#define SPP_LOGGER SPP::INSTANCE()

#ifdef NO_LOGGING	
	#define SPP_QL(...) 	
	#define SPP_LOG(...) 	
#else
	#define SPP_QL(F,...) SPP::INSTANCE().Log(SPP::LOG_SPP_GENERIC, SPP::LOG_INFO, __LINE__, __FILE__, F, ##__VA_ARGS__); 
	#define SPP_LOG(C,L,F,...) SPP::INSTANCE().Log(C, SPP::L, __LINE__, __FILE__, F, ##__VA_ARGS__); 	
#endif
