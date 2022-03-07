// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPLogging.h"
#include "ThreadPool.h"
#include "SPPTiming.h"
#include "SPPFileSystem.h"
#include "SPPPlatformCore.h"

#if PLATFORM_MAC
#include <mach-o/dyld.h>
#endif

SPP_OVERLOAD_ALLOCATORS

extern const char* kGitHash;
extern const char* kGitTag;

namespace SPP
{
	LogEntry LOG_CORE("CORE");
    static std::string GBinaryPath;

	void* SPP_MALLOC(std::size_t size)
	{
		void* ptr = malloc(size);
		return ptr;
	}
	void SPP_FREE(void* ptr)
	{
		free(ptr);
	}

	const char* GetGitHash()
	{		
		return kGitHash;
	}
	const char* GetGitTag()
	{		
		return kGitTag;
	}
    const char* GetBinaryDirectory()
    {
        return GBinaryPath.c_str();
    }
    
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

		auto ProcessName = GetProcessName();
        
#if PLATFORM_MAC
        {
            char path[2048];
            uint32_t size = sizeof(path);
            if (_NSGetExecutablePath(path, &size) == 0)
            {
                GBinaryPath = stdfs::path(path).remove_filename();
            }
        }
#else
        GBinaryPath = stdfs::current_path().generic_string();
#endif

        auto LoggingDirectory =   stdfs::weakly_canonical(stdfs::path(GBinaryPath) / "../Logging");
        stdfs::create_directories(LoggingDirectory);
        
		std::string ProcessNameAsLog = stdfs::path(ProcessName).stem().generic_string() + "_LOG.txt";
		SPP_LOGGER.Attach<FileLogger>( (LoggingDirectory / ProcessNameAsLog).generic_string().c_str() );
		
		SPP_LOG(LOG_CORE, LOG_INFO, "InitializeApplicationCore: cmd %s PWD: %s GIT HASH: %s GIT TAG: %s",
			Commandline ? Commandline : "NULL",
            GBinaryPath.c_str(),
			GetGitHash(),
			GetGitTag());
				
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
