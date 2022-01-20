// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
	
namespace SPP
{
	struct PlatformInfo
	{
		uint32_t PageSize;
		uint32_t ProcessorCount;
	};

	struct MonitorInfo
	{
		std::string Name;

		int32_t X;
		int32_t Y;
		int32_t Width;
		int32_t Height;
	};

	SPP_CORE_API std::string GetProcessName();

	SPP_CORE_API PlatformInfo GetPlatformInfo();
	SPP_CORE_API uint32_t CreateChildProcess(const char* ProcessPath, const char* Commandline, bool bStartVisible = true);
	SPP_CORE_API bool IsChildRunning(uint32_t processID);
	SPP_CORE_API void CloseChild(uint32_t processID);

	SPP_CORE_API void ShowMouse(bool bShowMouse);
	SPP_CORE_API void CaptureWindow(void *Window);

	SPP_CORE_API void AddDLLSearchPath(const char* InPath);

	SPP_CORE_API void GetMonitorInfo(std::vector<MonitorInfo> &oInfos);

	class PlatformProcess
	{
	protected:
		std::string _processPath;
		std::string _commandline;
		bool _bStartVisible = false;
		bool _bOutputToString = false;
	public:

		PlatformProcess(const char* ProcessPath, const char* Commandline, bool bStartVisible, bool bInPutToString) :
			_processPath(ProcessPath), _commandline(Commandline), _bStartVisible(bStartVisible), _bOutputToString(bInPutToString) { }

		virtual ~PlatformProcess() {} 

		virtual bool IsValid() = 0;
		virtual bool IsRunning() = 0;

		virtual std::string GetOutput() = 0;
	};

	SPP_CORE_API std::shared_ptr< PlatformProcess> CreatePlatformProcess(const char* ProcessPath, const char* Commandline = nullptr, bool bStartVisible = false, bool bInPutToString = false);
}

extern "C" SPP_CORE_API uint32_t C_CreateChildProcess(const char* ProcessPath, const char* Commandline, bool bStartVisible);
extern "C" SPP_CORE_API bool C_IsChildRunning(uint32_t processID);
extern "C" SPP_CORE_API void C_CloseChild(uint32_t processID);