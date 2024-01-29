// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <vector>

// copied and renamed from easy profiler to avoid define conflicts
#define SPP_STRINGIFY(a) #a
#define SPP_STRINGIFICATION(a) SPP_STRINGIFY(a)
#define SPP_TOKEN_JOIN(x, y) x ## y
#define SPP_TOKEN_CONCATENATE(x, y) SPP_TOKEN_JOIN(x, y)

// lazy wrappers

//EASY_THREAD_LOCAL static const char* EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__) = nullptr; \
//if (!EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__))\
//EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__) = ::profiler::registerThread(name);

//# define EASY_BLOCK(name, ...)\
//    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(::profiler::extract_enable_flag(__VA_ARGS__),\
//        EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name), __FILE__, __LINE__, ::profiler::BlockType::Block, ::profiler::extract_color(__VA_ARGS__),\
//        ::std::is_base_of<::profiler::ForceConstStr, decltype(name)>::value));\
//    ::profiler::Block EASY_UNIQUE_BLOCK(__LINE__)(EASY_UNIQUE_DESC(__LINE__), EASY_RUNTIME_NAME(name));\
//    ::profiler::beginBlock(EASY_UNIQUE_BLOCK(__LINE__));


//#define EASY_NONSCOPED_BLOCK(name, ...)\
//    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(::profiler::extract_enable_flag(__VA_ARGS__),\
//        EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name), __FILE__, __LINE__, ::profiler::BlockType::Block, ::profiler::extract_color(__VA_ARGS__),\
//        ::std::is_base_of<::profiler::ForceConstStr, decltype(name)>::value));\
//    ::profiler::beginNonScopedBlock(EASY_UNIQUE_DESC(__LINE__), EASY_RUNTIME_NAME(name));

namespace SPP
{
	SPP_CORE_API void ProfilerStartRecording();
	SPP_CORE_API void ProfilerFinishRecording();

	SPP_CORE_API void ProfilerStartListening();

	class SPP_CORE_API ProfilerThreadRegister
	{
	private:
		struct Impl;
		std::unique_ptr<Impl> _impl;

	public:
		ProfilerThreadRegister(const char* InName);
		~ProfilerThreadRegister();
	};

	class SPP_CORE_API ProfilerStat
	{
	private:
		const void* _statPtr = nullptr;
		std::string _name;
		std::string _file;
		std::string _uniqueID;

	public:
		ProfilerStat(const char* InName, const char* InFile, uint32_t InLine);

		auto GetStatPtr() const { return _statPtr; }
	};

	class SPP_CORE_API ScopedStatActivity
	{
	private:

		const ProfilerStat& _statRef;

	public:

		ScopedStatActivity(const ProfilerStat& InStatRef);
		~ScopedStatActivity();
	};
}

//Profile set thread
#define P_SET_THREAD(name) static SPP::ProfilerThreadRegister SPP_TOKEN_CONCATENATE(profThreadReg, __LINE__)(name);
//Profile scope
#define P_SCOPE(name) static SPP::ProfilerStat SPP_TOKEN_CONCATENATE(profStat, __LINE__)(name, __FILE__, __LINE__); \
		SPP::ScopedStatActivity SPP_TOKEN_CONCATENATE(profStatAct, __LINE__)(SPP_TOKEN_CONCATENATE(profStat, __LINE__)); 
