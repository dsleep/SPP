// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include <memory>
#include <cstdint>
#include <cstddef>

#if _WIN32 && !defined(SPP_CORE_STATIC)

	#ifdef SPP_CORE_EXPORT
		#define SPP_CORE_API __declspec(dllexport)
	#else
		#define SPP_CORE_API __declspec(dllimport)
	#endif

#else

	#define SPP_CORE_API 

#endif

// move to private include
#ifdef _MSC_VER
#pragma warning( disable: 4251 )
#pragma warning( disable: 4275 )
#pragma warning( disable: 4996 )
#endif

namespace SPP
{

	template <typename T, std::size_t N>
	constexpr std::size_t ARRAY_SIZE(const T(&)[N]) { return N; }

#define NO_CONSTRUCTION_ALLOWED(ClassName)				\
	ClassName() = delete;				

#define NO_COPY_ALLOWED(ClassName)						\
	ClassName(ClassName const&) = delete;				\
	ClassName& operator=(ClassName const&) = delete;

#define NO_MOVE_ALLOWED(ClassName)						\
	ClassName(ClassName&&) = delete;					\
	ClassName& operator=(ClassName&&) = delete;	

#define SE_CRASH_BREAK *reinterpret_cast<int32_t*>(3) = 0xDEAD;
#define SE_SYNCHRONIZED(M)  if (std::unique_lock<std::mutex> lck(M); lck.owns_lock())
#define SE_ASSERT(x) { if(!(x)) { SE_CRASH_BREAK } } 

	static constexpr uint8_t const THREAD_CPU = 0;
	static constexpr uint8_t const THREAD_IO = 1;
	static constexpr uint8_t const THREAD_GPU = 2;
	static constexpr uint8_t const THREAD_ENGINE = 3;

	SPP_CORE_API void IntializeCore(const char* Commandline);
	
	SPP_CORE_API extern std::unique_ptr<class ThreadPool> CPUThreaPool;
}

extern "C" SPP_CORE_API void C_IntializeCore();