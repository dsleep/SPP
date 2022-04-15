// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <coroutine>
#include <list>

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
	class coroutine_base;


	class scheduler_base
	{
	protected:
	public:
	};

	class simple_scheduler : public scheduler_base
	{
	protected:
		std::list< std::shared_ptr< coroutine_base > > _coroutines;
		std::shared_ptr< coroutine_base > _curActive;

	public:

		template<typename COROTYPE>
		void Schedule(COROTYPE &&InCoroutine);

		void RunOnce();
	};

	class coroutine_base 
	{
		friend class simple_scheduler;

	protected:
		simple_scheduler* _scheduler = nullptr;

	public:
		virtual bool resume() { return true; }
	};


	struct final_awaitable;

	template<typename RT>
	class coroutine;

	class coroutine_promise_base
	{
	public:
	};

	template<typename RT>
	class coroutine_promise : public coroutine_promise_base
	{
	public:
		coroutine_promise()
		{

		}

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

		coroutine<RT> get_return_object() noexcept;
	};

	template<>
	class coroutine_promise<void>
	{
	public:
		coroutine_promise()
		{

		}
		~coroutine_promise()
		{

		}
		using coro_handle = std::coroutine_handle<coroutine_promise<void>>;

		auto initial_suspend() noexcept { return std::suspend_always{}; }
		auto final_suspend() noexcept { return std::suspend_always{}; }
		void return_void() {}
		void unhandled_exception() { std::terminate(); }
		void result() {};

		coroutine<void> get_return_object() noexcept;
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
		using cororoutine_type = coroutine<RT>;
		using promise_type = coroutine_promise<RT>;
		using value_type = RT;
		using coro_handle = std::coroutine_handle<promise_type>;

		coroutine() noexcept
			: co_handle(nullptr)
		{}
		explicit coroutine(coro_handle InCoroutine)
			: co_handle(InCoroutine)
		{}

		coroutine(coroutine&& t) noexcept
			: co_handle(t.co_handle)
		{
			t.co_handle = nullptr;
		}

		coroutine(coroutine&) = delete;
		coroutine& operator=(const coroutine&) = delete;

		~coroutine() 
		{
			if (co_handle)
			{
				co_handle.destroy();
			}
			co_handle = nullptr;
		}

		coroutine& operator=(coroutine&& other) noexcept
		{
			if (std::addressof(other) != this)
			{
				if (co_handle)
				{
					co_handle.destroy();
				}

				co_handle = other.co_handle;
				other.co_handle = nullptr;
			}

			return *this;
		}

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
			cororoutine_type* _this = nullptr;
			coro_handle m_coroutine;
			simple_scheduler* _scheduler = nullptr;

			awaitable_base(cororoutine_type *InThis) noexcept
			{
				_this = InThis;
				m_coroutine = InThis->co_handle;
				_scheduler = InThis->_scheduler;
			}

			bool await_ready() const noexcept { return false; }

			template<typename PROMISE>
			bool await_suspend(std::coroutine_handle<PROMISE> awaitingCoroutine) noexcept
			{
				auto getPromise = awaitingCoroutine.promise();	
				_scheduler->Schedule(std::move(*_this));

				return false;				
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

			return awaitable( (cororoutine_type *)this );
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

			return awaitable( (cororoutine_type * )this );
		}

	private:
		coro_handle co_handle;
	}; 
		
	template<typename RT>
	coroutine<RT> coroutine_promise<RT>::get_return_object() noexcept
	{
		return coroutine<RT>{ coro_handle::from_promise(*this) };
	}

	coroutine<void> coroutine_promise<void>::get_return_object() noexcept
	{
		return coroutine<void>{ coro_handle::from_promise(*this) };
	}


	void simple_scheduler::RunOnce()
	{
		for (auto& routine : _coroutines)
		{
			_curActive = routine;
			routine->resume();
		}
	}

	template<typename COROTYPE>
	void simple_scheduler::Schedule(COROTYPE&& InCoroutine)
	{
		_coroutines.push_back(std::make_shared< COROTYPE >(std::move(InCoroutine)));
		_coroutines.back()->_scheduler = this;
	}

	SPP_COROUTINES_API uint32_t GetCoroutineVersion();
}
