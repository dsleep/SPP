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

int main(int argc, char* argv[])
{
    IntializeCore(nullptr);

    struct U1231 { };
    SingleRequestor<uint8_t, U1231> dataRequestor(10 * 1024);

    {
        auto MyChunk = dataRequestor.Get();

    }

    BuddyAllocator< uint8_t, U1231 > buddyData(10 * 1024 * 1024, 128);
	
    {
        auto MyChunk = buddyData.Get(1024);

    }

    return 0;
}
