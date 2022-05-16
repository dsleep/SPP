// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPLogging.h"
#include "SPPCoroutines.h"
#include "ThreadPool.h"

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

static std::thread::id GPUFakeID;
std::unique_ptr<ThreadPool> GPUFakePool;

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

coroutine_refence<int32_t> GetData()
{
    SPP_LOG(LOG_APP, LOG_INFO, "hmm no data");
    //co_await OneLowereven();
    SPP_LOG(LOG_APP, LOG_INFO, "hmm no data2");
    co_return 12;
}

coroutine_refence<void> DoConnect()
{
    SPP_LOG(LOG_APP, LOG_INFO, "Pass 1");
    auto value = co_await GetData();
    SPP_LOG(LOG_APP, LOG_INFO, "Pass 2");
    co_return;
}

class GPU_CALL;
class gpu_coroutine_promise
{
public:
    gpu_coroutine_promise() {}
    
    using coro_handle = std::coroutine_handle<gpu_coroutine_promise>;

    auto initial_suspend() noexcept
    {
        auto currentThreadID = std::this_thread::get_id();
        SE_ASSERT(GPUFakeID != currentThreadID);
        return std::suspend_always{};        
    }
    auto final_suspend() noexcept
    {
        return std::suspend_always{};
    }
    void return_void() {}
    void unhandled_exception() { std::terminate(); }
    void result() {};

    GPU_CALL get_return_object() noexcept;
};

class GPU_CALL
{
private:

public:
    using promise_type = gpu_coroutine_promise;
    using coro_handle = std::coroutine_handle<promise_type>;
    
    coro_handle _handle;

    GPU_CALL(coro_handle InHandle) : _handle(InHandle)
    {
        GPUFakePool->enqueue([InHandle]()
            {
                InHandle.resume();
            });
    }
};

GPU_CALL gpu_coroutine_promise::get_return_object() noexcept
{
    return GPU_CALL( coro_handle::from_promise(*this));
}

class TestThis
{
private:
    int32_t someData = 0;

public:


    GPU_CALL GPUCall()
    {
        SPP_LOG(LOG_APP, LOG_INFO, "ON GPU Thread");
        co_return;
    }
};



int main(int argc, char* argv[])
{
    IntializeCore(nullptr);

    GPUFakePool.reset(new ThreadPool(1));
    auto valueWait = GPUFakePool->enqueue([]()
        {
            GPUFakeID = std::this_thread::get_id();
            return true;
        });
    valueWait.wait();


    TestThis ourValue;

    ourValue.GPUCall();


    //auto currentSlice = ourValue.DoConnect(0);

    //while (currentSlice.resume())
    //{

    //}


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
