// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPEngine.h"
#include <coroutine>

#if _WIN32 && !defined(SPP_GRAPHICS_STATIC)
	#ifdef SPP_GRAPHICSE_EXPORT
		#define SPP_GRAPHICS_API __declspec(dllexport)
	#else
		#define SPP_GRAPHICS_API __declspec(dllimport)
	#endif
#else
	#define SPP_GRAPHICS_API 
#endif


namespace SPP
{
	SPP_GRAPHICS_API extern std::unique_ptr<class ThreadPool> GPUThreaPool;

	SPP_GRAPHICS_API void IntializeGraphics();	
	SPP_GRAPHICS_API bool IsOnGPUThread();

    class GPU_CALL;
    class gpu_coroutine_promise
    {
    public:
        gpu_coroutine_promise() {}

        using coro_handle = std::coroutine_handle<gpu_coroutine_promise>;

        auto initial_suspend() noexcept
        {
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
    public:  
        using promise_type = gpu_coroutine_promise;
        using coro_handle = std::coroutine_handle<promise_type>;

        GPU_CALL(coro_handle InHandle);
    };
}
