// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPCore.h"
#include "SPPLogging.h"
#include "SPPPlatformCore.h"

#include "SPPSTLUtils.h"
#include "SPPString.h"

#include <thread>
#include <chrono>
using namespace SPP;

LogEntry LOG_APP("APP");

using namespace std::chrono_literals;

std::map<std::string, std::string> BuildCCMap(int argc, const char* argv[])
{
	std::map<std::string, std::string> oMap;

	for (int ArgIter = 0; ArgIter < argc; ArgIter++)
	{
		std::string curArg = argv[ArgIter];

		auto trimmed = trim(curArg);

		if (trimmed.length() > 4 && trimmed[0] == '-')
		{
			auto curEquals = trimmed.find_first_of('=');
			if (curEquals != std::string::npos &&
				curEquals > 1)
			{
				auto curKey = trimmed.substr(1, curEquals - 1);
				auto curValue = trimmed.substr(curEquals + 1);
				oMap[curKey] = curValue;
			}
		}
	}

	return oMap;
}

class DeadlockIPCWatch
{
public:
	void InitializeMonitor()
	{

	}
	void ReportToAppMonitor()
	{

	}

};

int main(int argc, char* argv[])
{
	IntializeCore(nullptr);

	auto CCMap = BuildCCMap(argc, argv);
	
	while (true)
	{		
	

		std::this_thread::sleep_for(1ms);
	}

	return 0;
}
