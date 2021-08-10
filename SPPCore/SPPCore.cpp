// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPLogging.h"
#include "ThreadPool.h"
#include "SPPTiming.h"
#include "SPPFileSystem.h"

#if _WIN32
	#include "SPPWin32Core.h"
#endif

namespace SPP
{
	LogEntry LOG_CORE("CORE");

	SPP_CORE_API std::unique_ptr<class ThreadPool> CPUThreaPool;

	static SystemClock::time_point appStarted;

	double TimeSinceAppStarted()
	{
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(SystemClock::now() - appStarted).count();
		return (double)millis / 1000.0;
	}

	void IntializeCore(const char* Commandline)
	{
		appStarted = SystemClock::now();

		SPP_LOGGER.Attach<ConsoleLog>();
		SPP_LOGGER.SetLogLevel(LOG_INFO);

#if _WIN32
		auto ProcessName = GetProcessName();
		std::string ProcessNameAsLog = stdfs::path(ProcessName).stem().generic_string() + "_LOG.txt";
		SPP_LOGGER.Attach<FileLogger>(ProcessNameAsLog.c_str());
#endif
		
		SPP_LOG(LOG_CORE, LOG_INFO, "InitializeApplicationCore: cmd %", Commandline ? Commandline : "NULL");
				
		unsigned int nthreads = std::max<uint32_t>( std::thread::hardware_concurrency(), 2);

		CPUThreaPool = std::make_unique< ThreadPool >(nthreads - 1);

		auto Info = GetPlatformInfo();

		SPP_LOG(LOG_CORE, LOG_INFO, " - core count %u", nthreads);
		SPP_LOG(LOG_CORE, LOG_INFO, " - page size %u", Info.PageSize);

		srand((unsigned int)time(NULL));
	}
}


void C_IntializeCore()
{
	SPP::IntializeCore("");
}