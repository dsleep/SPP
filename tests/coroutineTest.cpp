// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPLogging.h"
#include "SPPCoroutines.h"

using namespace SPP;

LogEntry LOG_APP("APP");
//
//struct CoroutineComplete
//{
//    std::atomic_bool bComplete{ false };
//};
//
//coroutine<void> CorourtineSet(std::shared_ptr<CoroutineComplete> simpleComplete)
//{
//    simpleComplete->bComplete = true;
//    co_return;
//}
//
//coroutine<void> CorourtineWait(std::shared_ptr<CoroutineComplete> simpleComplete)
//{
//    while (!simpleComplete->bComplete)
//    {
//        co_await std::suspend_always{};
//    }
//    co_return;
//}
//
//struct simple_awaitable
//{
//    simple_scheduler* scheduler = nullptr;
//
//    bool await_ready() { return false; }
//
//    template<typename PROMISE>
//    void await_suspend(std::coroutine_handle<PROMISE> awaitingCoroutine)
//    {
//        auto& getPromise = awaitingCoroutine.promise();
//        auto simpleComplete = std::make_shared<CoroutineComplete>();
//
//        auto prevScheduler = getPromise.GetScheduler();
//
//        auto taskSetter = CorourtineSet(simpleComplete);
//        auto taskWaiter = CorourtineWait(simpleComplete);
//        
//        prevScheduler->MoveCurrent(std::move(taskWaiter));
//        prevScheduler->Schedule(std::move(taskWaiter));
//        
//        scheduler->Schedule(std::move(taskWaiter));
//    }
//    void await_resume() {}
//};
//
//auto switch_scheduler(simple_scheduler *NewScheduler)
//{
//    return simple_awaitable{ NewScheduler };
//}

coroutine_refence<void> OneLowereven()
{
    int thisCount = 0;
    while (thisCount < 10)
    {
        co_await std::suspend_always{};
        thisCount++;
    }
    SPP_LOG(LOG_APP, LOG_INFO, "OneLowereven");

    co_return;
}

coroutine_refence<void> GetData()
{
    SPP_LOG(LOG_APP, LOG_INFO, "hmm no data");

    co_await OneLowereven();
    SPP_LOG(LOG_APP, LOG_INFO, "hmm no data2");

    co_return;
}

coroutine_refence<void> DoConnect()
{
    SPP_LOG(LOG_APP, LOG_INFO, "Pass 1");
    co_await GetData();
    SPP_LOG(LOG_APP, LOG_INFO, "Pass 2");
    co_return;
}



int main(int argc, char* argv[])
{
    IntializeCore(nullptr);

    auto thisValue = DoConnect();

    while (thisValue.resume())
    {

    }
    //simple_scheduler scheduler;
    //scheduler.Schedule(DoConnect());
    //while (scheduler.RunOnce())
    //{

    //}

    return 0;
}
