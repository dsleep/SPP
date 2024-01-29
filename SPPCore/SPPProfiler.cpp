// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPProfiler.h"
#include "SPPLogging.h"
#include "SPPPlatformCore.h"

#include <functional>

#define USING_EASY_PROFILER

// encapsulate? 
#include <easy/profiler.h>
#include <easy/arbitrary_value.h>
#include <easy/reader.h>


namespace SPP
{
	struct EZDllPath
	{
		EZDllPath()
		{
			AddDLLSearchPath("../3rdParty/easy_profiler/bin");
		}
	};	

	static EZDllPath GEZDllPath;

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

	void ProfilerStartRecording()
	{
		::profiler::setEnabled(true);
	}

	void ProfilerFinishRecording()
	{
		// also disables profiler
		if (::profiler::isEnabled())
		{
			std::string profileFilename = std::string("P") + GetTimeStampTag() + ".prof";

			auto profileFullPath = stdfs::path(GetProfilingPath()) / profileFilename;
			::profiler::dumpBlocksToFile(profileFullPath.generic_string().c_str());
		}
	}

	void ProfilerStartListening()
	{
		::profiler::startListen();
	}

	struct ProfilerThreadRegister::Impl
	{
		std::string name;
		::profiler::ThreadGuard guard;
	};

	ProfilerThreadRegister::ProfilerThreadRegister(const char* InName) : _impl(new Impl())
	{
		_impl->name = InName;
		::profiler::registerThreadScoped(_impl->name.c_str(), _impl->guard);		
	}

	ProfilerThreadRegister::~ProfilerThreadRegister()
	{
	}

	ProfilerStat::ProfilerStat(const char* InName, const char* InFile, uint32_t InLine)
	{
		_name = InName;
		_file = InFile;
		_uniqueID = std::string_format("%s:%d", InFile, InLine);

		auto hashValue = std::hash<uint32_t>{}(InLine);

		uint8_t red = uint8_t((hashValue & 0xFF000000) >> 24);
		uint8_t green = uint8_t((hashValue & 0x00FF0000) >> 16);
		uint8_t blue = uint8_t((hashValue & 0x0000FF00) >> 8);

		::profiler::color_t color = ::profiler::colors::color(red, green, blue);

		_statPtr = ::profiler::registerDescription(::profiler::ON,
			_uniqueID.c_str(),
			_name.c_str(),
			InFile, InLine, 
			::profiler::BlockType::Block, 
			color);
	}


	ScopedStatActivity::ScopedStatActivity(const ProfilerStat& InStatRef) : _statRef(InStatRef)
	{
		::profiler::beginNonScopedBlock((const ::profiler::BaseBlockDescriptor * )_statRef.GetStatPtr());
	}

	ScopedStatActivity::~ScopedStatActivity()
	{
		::profiler::endBlock();
	}
}
