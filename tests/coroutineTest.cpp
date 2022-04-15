// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPLogging.h"
#include "SPPCoroutines.h"


using namespace SPP;

LogEntry LOG_APP("APP");

coroutine<void> GetData()
{
    SPP_LOG(LOG_APP, LOG_INFO, "hmm no data");

    co_return;
}

coroutine<void> DoConnect()
{
    SPP_LOG(LOG_APP, LOG_INFO, "Pass 1");

    co_await GetData();

    SPP_LOG(LOG_APP, LOG_INFO, "Pass 2");

    co_return;
}



int main(int argc, char* argv[])
{
    IntializeCore(nullptr);

    {
        auto scopeTest = DoConnect();

    }

    simple_scheduler scheduler;
    scheduler.Schedule(DoConnect());
    scheduler.RunOnce();

    return 0;
}
