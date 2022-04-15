// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <coroutine>

#if _WIN32 && !defined(SPP_COROUTINES_STATIC)

	#ifdef SPP_COROUTINES_EXPORT
		#define SPP_COROUTINES_API __declspec(dllexport)
	#else
		#define SPP_COROUTINES_API __declspec(dllimport)
	#endif

#else

	#define SPP_COROUTINES_API 

#endif

namespace SPP
{	
	class scheduler_base
	{
	protected:
	public:
	};

	class simple_scheduler : public scheduler_base
	{
	protected:
	public:

		void run()
		{

		}
	};

	class coroutine_base 
	{
	protected:
		std::shared_ptr< simple_scheduler > _scheduler;

	public:
		virtual bool resume() { return true; }
	};

	struct final_awaitable;

	class coroutine_promise_base
	{
	public:
	};

	template<typename RT>
	class coroutine_promise : public coroutine_promise_base
	{
	public:
		using coro_handle = std::coroutine_handle<coroutine_promise<RT>>;

		auto initial_suspend() noexcept { return std::suspend_always{}; }
		auto final_suspend() noexcept { return std::suspend_always{}; }

		// void return_void() {}

		// Place to hold the results produced by the coroutine
		RT data;

		void return_value(const RT& value) noexcept { data = std::move(value); }
		void unhandled_exception() { std::terminate(); }

		RT result()
		{
			return data;
		}
	};

	template<>
	class coroutine_promise<void>
	{
	public:
		auto initial_suspend() noexcept { return std::suspend_always{}; }
		auto final_suspend() noexcept { return std::suspend_always{}; }
		void return_void() {}
		void unhandled_exception() { std::terminate(); }
		void result() {};
	};

	class broken_promise : public std::logic_error
	{
	public:
		broken_promise()
			: std::logic_error("broken promise")
		{
		}
	};

	template<typename RT = void>
	class coroutine : public coroutine_base
	{
	public:
		using promise_type = coroutine_promise<RT>;
		using value_type = RT;
		using coro_handle = std::coroutine_handle<promise_type>;

		coroutine(coro_handle& handle)
			: co_handle(handle)
		{
			assert(handle);
		}
		coroutine(coroutine&) = delete;
		coroutine(coroutine&&) = delete;

		~coroutine() { co_handle.destroy(); }

		virtual bool resume() override
		{
			if (!co_handle.done())
			{
				co_handle.resume();
			}
			return !co_handle.done();
		}

		struct awaitable_base
		{
			coro_handle m_coroutine;

			awaitable_base(coro_handle coroutine) noexcept
				: m_coroutine(coroutine)
			{
			}

			bool await_ready() const noexcept { return !m_coroutine || m_coroutine.done(); }

			template<typename PROMISE>
			bool await_suspend(std::coroutine_handle<PROMISE> awaitingCoroutine) noexcept
			{
				//coroutinebase& promise = awaitingCoroutine.promise();
				//promise._nestedcoroutine = awaitingCoroutine;
				return false;
				//m_coroutine.promise().try_set_continuation(awaitingCoroutine);
			}
		};

		auto operator co_await() const& noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				decltype(auto) await_resume()
				{
					if (!this->m_coroutine)
					{
						throw broken_promise{};
					}

					return this->m_coroutine.promise().result();
				}
			};

			return awaitable{ co_handle };
		}

		auto operator co_await() const&& noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				decltype(auto) await_resume()
				{
					if (!this->m_coroutine)
					{
						throw broken_promise{};
					}

					return std::move(this->m_coroutine.promise()).result();
				}
			};

			return awaitable{ co_handle };
		}

	private:
		coro_handle co_handle;
	};

	SPP_COROUTINES_API uint32_t GetCoroutineVersion();
}
