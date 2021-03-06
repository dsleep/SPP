// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPLogging.h"
#include "ThreadPool.h"
#include "SPPTiming.h"
#include "SPPFileSystem.h"
#include "SPPPlatformCore.h"
#include "SPPStackUtils.h"
#include "SPPString.h"

#if PLATFORM_MAC || PLATFORM_LINUX
    #include <sys/param.h>
    #include <unistd.h>
#endif

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
    static std::string GResourcePath;
	static std::string GLogPath;

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
    const char* GetResourceDirectory()
    {
        return GResourcePath.c_str();
    }
	const char* GetLogPath()
	{
		return GLogPath.c_str();
	}
	SPP_CORE_API std::unique_ptr<class ThreadPool> CPUThreaPool;

	static SystemClock::time_point appStarted;

	double TimeSinceAppStarted()
	{
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(SystemClock::now() - appStarted).count();
		return (double)millis / 1000.0;
	}

	void CleanupOldLogs(const stdfs::path &LoggingDirectory)
	{
		try
		{
			//3 days ago
			const auto DaysAgoTimeFS = stdfs::file_time_type::clock::now().time_since_epoch() - std::chrono::hours(24*3);
			std::vector< stdfs::path > filesToDelete;
			for (auto const& dir_entry : std::filesystem::directory_iterator{ LoggingDirectory })
			{
				if (stdfs::is_regular_file(dir_entry))
				{
					const std::string file_ext = dir_entry.path().extension().string();

					if (std::str_equals(file_ext, ".txt"))
					{
						auto ftimeduration = stdfs::last_write_time(dir_entry).time_since_epoch();

						if (ftimeduration < DaysAgoTimeFS)
						{
							filesToDelete.push_back(dir_entry);
						}
					}
				}
			}
			SPP_LOG(LOG_CORE, LOG_INFO, "Deleting %d old logs...", filesToDelete.size());
			for (auto const& fileIter : filesToDelete)
			{
				stdfs::remove(fileIter);
			}
		}
		catch (std::exception&) {}
	}

	static std::thread::id CPUThread;

	bool IsOnCPUThread()
	{
		// make sure its an initialized one
		SE_ASSERT(CPUThread != std::thread::id());
		auto currentThreadID = std::this_thread::get_id();
		return (CPUThread == currentThreadID);
	}

	void IntializeCore(const char* Commandline)
	{
		appStarted = SystemClock::now();

		SPP_LOGGER.Attach<ConsoleLog>();
		SPP_LOGGER.SetLogLevel(LOG_INFO);

		auto ProcessName = GetProcessName();
        
        SignalHandlerInit();
        
#if PLATFORM_MAC
        {
            char path[2048];
            uint32_t size = sizeof(path);
            if (_NSGetExecutablePath(path, &size) == 0)
            {
                GBinaryPath = stdfs::path(path).parent_path();
                GResourcePath = stdfs::path(GBinaryPath).parent_path() / "Resources";
            }
        }
        stdfs::current_path(GBinaryPath);
        chdir(GBinaryPath.c_str());
        
        SPP_LOG(LOG_CORE, LOG_INFO, "InitializeApplicationCore: making sure PWD is binary path %s", GBinaryPath.c_str());
#else
        GBinaryPath = stdfs::current_path().generic_string();
#endif

        auto LoggingDirectory =   (stdfs::path(GBinaryPath) / "../Logging").lexically_normal();
        stdfs::create_directories(LoggingDirectory);
        
		std::string buffer(128, '\0');
		auto timeTApp = std::chrono::system_clock::to_time_t(appStarted);
		auto appLT = localtime(&timeTApp);

		strftime(&buffer[0], buffer.size(), "_%Y-%m-%d@%H-%M_LOG.txt", appLT);

		std::string ProcessNameAsLog = stdfs::path(ProcessName).stem().generic_string() + buffer;
		stdfs::path LogFullPath = (LoggingDirectory / ProcessNameAsLog);
		GLogPath = LogFullPath.generic_string();
		SPP_LOGGER.Attach<FileLogger>(LogFullPath.generic_string().c_str() );
		
		SPP_LOG(LOG_CORE, LOG_INFO, "InitializeApplicationCore: cmd %s PWD: %s GIT HASH: %s GIT TAG: %s",
			Commandline ? Commandline : "NULL",
            GBinaryPath.c_str(),
			GetGitHash(),
			GetGitTag());

		//delete old logs
		CleanupOldLogs(LoggingDirectory);
				
		unsigned int nthreads = std::max<uint32_t>( std::thread::hardware_concurrency(), 2);

		CPUThreaPool = std::make_unique< ThreadPool >(nthreads - 1);
		CPUThread = std::this_thread::get_id();

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
