// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPReferenceCounter.h"
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

	template<typename RT>
	class coroutine_refence;

	class scheduler_base
	{
	protected:
	public:
	};

	struct coroutinestack
	{
		std::list< std::shared_ptr< coroutine_base > > _coroutines;
	};

	class simple_scheduler : public scheduler_base
	{
	protected:
		std::list< std::unique_ptr< coroutinestack > > _activeCoroutines;
		coroutinestack *_activeStack = nullptr;

	public:

		template<typename COROTYPE>
		void Schedule(COROTYPE &&InCoroutine);

		bool MoveCurrent(simple_scheduler &NewScheduler);

		bool RunOnce();
	};

	class coroutine_base_refence : public Referencer< coroutine_base >
	{
	public:
		coroutine_base_refence(coroutine_base* obj = nullptr) : Referencer< coroutine_base >(obj)
		{
		}

		bool resume();

		template<typename T>
		void operator= (const coroutine_refence<T>& right)
		{
			coroutine_base* thisObj = right.RawGet();
			Referencer< coroutine_base >::operator=(thisObj);
		}
	};

	class coroutine_base : public ReferenceCounted
	{
		friend class simple_scheduler;

	public:
		coroutine_base_refence _nestedCoroutine;

	public:
		
		virtual bool resume() { return true; }
	};


	bool coroutine_base_refence::resume()
	{
		if (this->obj)
		{
			return this->obj->resume();
		}
		return {};
	}

	struct final_awaitable;

	template<typename RT>
	class coroutine;

	class coroutine_promise_base
	{
	protected:
		simple_scheduler* _scheduler = nullptr;
		coroutine_base* _owning_coroutine = nullptr;
	public:
		void SetScheduler(simple_scheduler* InScheduler)
		{
			_scheduler = InScheduler;
		}
		simple_scheduler* GetScheduler()
		{
			return _scheduler;
		}
		void SetOwner(coroutine_base* InCoro)
		{
			_owning_coroutine = InCoro;
		}
		coroutine_base* GetOwner() const
		{
			return _owning_coroutine;
		}
		virtual ~coroutine_promise_base() { }
	};

	template<typename RT>
	class coroutine_promise : public coroutine_promise_base
	{
	public:
		coroutine_promise()
		{

		}
		virtual ~coroutine_promise()
		{

		}

		using coro_handle = std::coroutine_handle<coroutine_promise<RT>>;

		auto initial_suspend() noexcept { return std::suspend_always{}; }
		auto final_suspend() noexcept { return std::suspend_always{}; }

		// void return_void() {}

		// Place to hold the results produced by the coroutine
		RT data{};

		void return_value(const RT& value) noexcept { data = value; }
		void unhandled_exception() { std::terminate(); }

		RT result()
		{
			return data;
		}

		coroutine_refence<RT> get_return_object() noexcept;
	};

	template<>
	class coroutine_promise<void> : public coroutine_promise_base
	{
	public:
		coroutine_promise()
		{

		}
		virtual ~coroutine_promise()
		{

		}

		using coro_handle = std::coroutine_handle<coroutine_promise<void>>;

		auto initial_suspend() noexcept { return std::suspend_always{}; }
		auto final_suspend() noexcept { return std::suspend_always{}; }
		void return_void() {}
		void unhandled_exception() { std::terminate(); }
		void result() {};

		coroutine_refence<void> get_return_object() noexcept;
	};

	class broken_promise : public std::logic_error
	{
	public:
		broken_promise()
			: std::logic_error("broken promise")
		{
		}
	};

	class coroutine_base_refence;

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
			if (_nestedCoroutine)
			{
				if (!_nestedCoroutine->resume())
				{
					_nestedCoroutine.Reset();
				}
				return true;
			}
			if (!co_handle.done())
			{
				co_handle.resume();
			}
			return !co_handle.done();
		}

		coro_handle GetCoroutineHandle()
		{
			return co_handle;
		}

	private:
		coro_handle co_handle;
	}; 

	
		
	template<typename RT = void>
	class coroutine_refence : public Referencer< coroutine<RT> >
	{
	private:
				
	public:
		using cororoutine_ref_type = coroutine_refence<RT>;
		using cororoutine_type = coroutine<RT>;
		using promise_type = coroutine_promise<RT>;
		using value_type = RT;
		using coro_handle = std::coroutine_handle<promise_type>;

		coroutine_refence(cororoutine_type* obj = nullptr) : Referencer< coroutine<RT> >(obj)
		{
		}

		void operator= (const coroutine_refence<void>& right)
		{
			Referencer< coroutine<RT> >::operator = (right);
		}

		bool IsValid()
		{
			return (this->obj && this->obj->GetCoroutineHandle());
		}

		coro_handle GetCoroutineHandle() const
		{
			if (this->obj)
			{
				return this->obj->GetCoroutineHandle();
			}
			return {};
		}

		bool resume() 
		{
			if (this->obj)
			{
				return this->obj->resume();
			}
			return {};
		}

		struct awaitable_base
		{
			const cororoutine_ref_type _this;

			awaitable_base(const cororoutine_ref_type InThis) noexcept : _this(InThis)
			{
				
			}

			bool await_ready() const noexcept { return false; }

			template<typename PROMISE>
			bool await_suspend(std::coroutine_handle<PROMISE> awaitingCoroutine) noexcept
			{
				auto& getPromise = awaitingCoroutine.promise();
				auto owner = getPromise.GetOwner();
				SE_ASSERT(owner);
				owner->_nestedCoroutine = _this;
				return true;
			}
		};

		auto operator co_await() const& noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				decltype(auto) await_resume()
				{
					auto thisCoro = _this.GetCoroutineHandle();

					if (!thisCoro)
					{
						throw broken_promise{};
					}

					return thisCoro.promise().result();
				}
			};

			return awaitable(*this);
		}

		auto operator co_await() const&& noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				auto await_resume()
				{
					auto thisCoro = _this.GetCoroutineHandle();

					if (!thisCoro)
					{
						throw broken_promise{};
					}

					return std::move(thisCoro.promise()).result();
				}
			};

			return awaitable(*this);
		}
	};

	template<typename RT>
	coroutine_refence<RT> coroutine_promise<RT>::get_return_object() noexcept
	{
		coroutine_refence<RT> ref(new coroutine<RT>{ coro_handle::from_promise(*this) });
		SetOwner(ref.get());
		return ref;
	}

	coroutine_refence<void> coroutine_promise<void>::get_return_object() noexcept
	{
		coroutine_refence<void> ref(new coroutine<void>{ coro_handle::from_promise(*this) });
		SetOwner(ref.get());
		return ref;
	}

	bool simple_scheduler::RunOnce()
	{
		bool bAnyLeft = false;
		for (auto& coroStack : _activeCoroutines)
		{
			_activeStack = coroStack.get();

			auto thisCoro = _activeStack->_coroutines.back();
			if (!thisCoro->resume())
			{
				_activeStack->_coroutines.pop_back();
			}

			bAnyLeft |= !_activeStack->_coroutines.empty();
			_activeStack = nullptr;
		}
		return bAnyLeft;
	}

	bool simple_scheduler::MoveCurrent(simple_scheduler& NewScheduler)
	{
		auto currentCoro = _activeStack->_coroutines.back();

		NewScheduler._activeCoroutines.push_back(std::make_unique< coroutinestack >());
		NewScheduler._activeCoroutines.back()->_coroutines.push_back(currentCoro);

		_activeStack->_coroutines.pop_back();

		return true;
	}

	template<typename COROTYPE>
	void simple_scheduler::Schedule(COROTYPE&& InCoroutine)
	{
		auto& coroPromise = InCoroutine.GetCoroutineHandle().promise();
		coroutinestack* addtostack = _activeStack;
		if (addtostack == nullptr)
		{
			_activeCoroutines.push_back(std::make_unique< coroutinestack >());
			addtostack = _activeCoroutines.back().get();
		}
		addtostack->_coroutines.push_back(std::make_shared< COROTYPE >(std::move(InCoroutine)));
		coroPromise.SetScheduler(this);
	}

	SPP_COROUTINES_API uint32_t GetCoroutineVersion();
}
