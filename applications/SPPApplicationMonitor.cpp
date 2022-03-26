// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPCore.h"
#include "SPPLogging.h"
#include "SPPPlatformCore.h"

#include "SPPFileSystem.h"
#include "SPPMemory.h"
#include "SPPSTLUtils.h"
#include "SPPString.h"

#include <thread>
#include <chrono>
using namespace SPP;

LogEntry LOG_APP("APP");

using namespace std::chrono_literals;


int main(int argc, char* argv[])
{
	IntializeCore(nullptr);

	SPP_LOG(LOG_APP, LOG_INFO, "Application Monitor");

	auto CCMap = std::BuildCCMap(argc, argv);

	auto appPath = MapFindOrNull(CCMap, "app");
	auto appCL = MapFindOrNull(CCMap, "CL");
	auto watchDeadLock = MapFindOrNull(CCMap, "watchdeadlock");
	auto isHidden = MapFindOrNull(CCMap, "hide");
	
	if (appPath)
	{
		if (!stdfs::exists(appPath->c_str()))
		{
			SPP_LOG(LOG_APP, LOG_INFO, "PROCSES DOES NOT EXIST", appPath->c_str());
			return -1;
		}
		bool bDeadLock = watchDeadLock ? true : false;

		std::unique_ptr< IPCDeadlockCheck > deadlockCheck;
		std::string DeadLockString;
		if (bDeadLock)
		{
			deadlockCheck = std::make_unique< IPCDeadlockCheck>();
			DeadLockString = deadlockCheck->InitializeMonitor();
		}

		std::string appCC = appCL ? appCL->c_str() : "";

		if (!DeadLockString.empty())
		{
			appCC += " -deadlockmem=" + DeadLockString;
		}
		
		auto childRun = [appPathC = *appPath, appCC, isHidden]() -> uint32_t
		{
			return CreateChildProcess(appPathC.c_str(),
				appCC.c_str(),
				(isHidden == nullptr));
		};

		SPP_LOG(LOG_APP, LOG_INFO, " - process path %s", appPath->c_str());
		SPP_LOG(LOG_APP, LOG_INFO, " - process CL %s", appCC.c_str());

		auto childProcessID = childRun();

		auto lastDLUpdate = std::chrono::steady_clock::now();

		while (true)
		{
			auto thisTick = std::chrono::steady_clock::now();

			if (!deadlockCheck || deadlockCheck->CheckReporter())
			{
				lastDLUpdate = thisTick;
			}

			auto bchildrunning = IsChildRunning(childProcessID);
			auto bisChildDeadLocked = (std::chrono::duration_cast<std::chrono::seconds>(thisTick - lastDLUpdate).count() > 7);

			if (!bchildrunning || bisChildDeadLocked)
			{
				SPP_LOG(LOG_APP, LOG_INFO, "RESTARTING CHILD %d(not running) %d(deadlock)", bchildrunning, bisChildDeadLocked);
				//was deadlocked
				if (bchildrunning)
				{
					CloseChild(childProcessID);
				}
				childProcessID = childRun();
				lastDLUpdate = thisTick;
				std::this_thread::sleep_for(3s);
			}

			std::this_thread::sleep_for(1ms);
		}
	}

	return 0;
}
