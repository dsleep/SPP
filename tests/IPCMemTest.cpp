// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPString.h"
#include "SPPSTLUtils.h"
#include "SPPLogging.h"
#include "SPPWin32Core.h"
#include "SPPMemory.h"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;
using namespace SPP;

LogEntry LOG_APP("APP");


bool ParseCC(const std::string& InCmdLn, const std::string& InValue, std::string& OutValue)
{
	if (StartsWith(InCmdLn, InValue))
	{
		OutValue = std::string(InCmdLn.begin() + InValue.size(), InCmdLn.end());
		return true;
	}
	return false;
}

struct IPCMotionState
{
	int32_t buttonState[2];
	float motionXY[2];
	float orientationQuaternion[4];
};

int main(int argc, char* argv[])
{
    IntializeCore(nullptr);


    std::string IPMemoryID;

	for (int i = 0; i < argc; ++i)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "CC(%d):%s", i, argv[i]);

		auto Arg = std::string(argv[i]);
		ParseCC(Arg, "-MEM=", IPMemoryID);
	}


	std::unique_ptr<IPCMappedMemory> ipcMem; 
	std::unique_ptr< SimpleIPCMessageQueue<IPCMotionState> > _msgQueue;

	uint32_t BuzzTest = 0;
	bool bOwner = false;

	if (IPMemoryID.empty())
	{
		IPMemoryID = std::generate_hex(3);				
		CreateChildProcess("IPCMemTestd.exe", std::string_format("-MEM=%s", IPMemoryID.c_str()).c_str());

		ipcMem = std::make_unique< IPCMappedMemory>( IPMemoryID.c_str(), 1 * 1024 * 1024, true);
		bOwner = true;
	}
	else
	{
		ipcMem = std::make_unique< IPCMappedMemory>(IPMemoryID.c_str(), 1 * 1024 * 1024, false);
	}

	_msgQueue = std::make_unique< SimpleIPCMessageQueue<IPCMotionState> >(*ipcMem, sizeof(BuzzTest));

	while (true)
	{
		if (bOwner)
		{
			static int32_t IDCount = 0;
			IPCMotionState newMessage;
			newMessage.buttonState[0] = IDCount++;
			newMessage.buttonState[1] = IDCount++;
			_msgQueue->PushMessage(newMessage);
		}
		else
		{
			auto messages = _msgQueue->GetMessages();
			std::this_thread::sleep_for(200ms);
			SPP_LOG(LOG_APP, LOG_INFO, "messages %d", messages.size())
		}

		std::this_thread::sleep_for(33ms);
	}

    return 0;
}
