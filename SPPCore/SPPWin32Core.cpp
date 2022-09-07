// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPPlatformCore.h"
#include "SPPString.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"

#include <thread>
#include <mutex>
#include <list>
#include <stdio.h>
#include <stdarg.h>
#include <memory>
#include <ostream>
#include <optional>
#include <fstream>
#include <sstream>
#include <map>

#include "Windows.h"
#include "sysinfoapi.h"

namespace SPP
{
	SPP_CORE_API LogEntry LOG_WIN32CORE("WIN32CORE");

	std::string GetProcessName()
	{
		char Filename[MAX_PATH]; //this is a char buffer
		GetModuleFileNameA(GetModuleHandle(nullptr), Filename, sizeof(Filename));
		return Filename;
	}

	void SetThreadName(const char* InName)
	{
		SetThreadDescription(GetCurrentThread(), std::utf8_to_wstring(InName).c_str());
	}

	PlatformInfo GetPlatformInfo()	
	{
		SYSTEM_INFO info = { 0 };
		GetSystemInfo( &info );

		PlatformInfo oInfo = { info.dwPageSize, info.dwNumberOfProcessors };
		return oInfo;
	}

	BOOL CALLBACK MyInfoEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
	{
		MONITORINFOEX iMonitor;
		iMonitor.cbSize = sizeof(MONITORINFOEX);
		GetMonitorInfo(hMonitor, &iMonitor);

		if (iMonitor.dwFlags == DISPLAY_DEVICE_MIRRORING_DRIVER)
		{
			return true;
		}
		else
		{
			std::vector<MonitorInfo>* info = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);
			info->push_back({
				std::wstring_to_utf8(iMonitor.szDevice),
				iMonitor.rcMonitor.left,
				iMonitor.rcMonitor.top,
				iMonitor.rcMonitor.right - iMonitor.rcMonitor.left,
				iMonitor.rcMonitor.bottom - iMonitor.rcMonitor.left });
			return true;
		};
	}

	SPP_CORE_API void GetMonitorInfo(std::vector<MonitorInfo>& oInfos)
	{
		EnumDisplayMonitors(NULL, NULL, &MyInfoEnumProc, reinterpret_cast<LPARAM>(&oInfos));
	}

	
	
	class Win32Process : public PlatformProcess
	{
	private:
		uint32_t _processID = 0;
		
		HANDLE _childStd_OUT_Rd = NULL;
		HANDLE _childStd_OUT_Wr = NULL;

		std::thread _outputThread;
		std::shared_ptr< PROCESS_INFORMATION> pi;

		std::mutex _outputMutex;
		std::string _outputString;

	public:

		Win32Process(const char* ProcessPath, const char* Commandline, bool bStartVisible, bool bInputToString) :
			PlatformProcess(ProcessPath, Commandline, bStartVisible, bInputToString)
		{
			std::string stringProcessPath = _processPath;
			stdfs::path asPath(_processPath);
			if (!asPath.has_extension())
			{
				stringProcessPath += ".exe";
			}
			const char* CorrectedProcessPath = stringProcessPath.c_str();

			SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: %s %s", CorrectedProcessPath, Commandline);

			BOOL bIsProcessInJob = false;
			BOOL bSuccess = IsProcessInJob(GetCurrentProcess(), NULL, &bIsProcessInJob);
			if (bSuccess == 0)
			{
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: IsProcessInJob failed: error %d", GetLastError());
				return;
			}

			bool bCreateJob = true;

			HANDLE hJob = nullptr;
			if (bIsProcessInJob)
			{
				bCreateJob = false;
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: already in job");

				JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
				QueryInformationJobObject(NULL, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli), NULL);

				SPP_LOG(LOG_WIN32CORE, LOG_INFO, " -  silent break away %d", (jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK));
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, " -  kill on close %d", (jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, " -  break away %d", (jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK));

				bCreateJob = (jeli.BasicLimitInformation.LimitFlags & (JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK)) != 0;

				SPP_LOG(LOG_WIN32CORE, LOG_INFO, " - bCreateJob %d", bCreateJob);
			}

			if (bCreateJob)
			{
				hJob = CreateJobObject(NULL, NULL);
				if (hJob == NULL)
				{
					SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateJobObject failed : error 0x%X", GetLastError());
					return;
				}

				JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
				jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
				bSuccess = SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
				if (bSuccess == 0) 
				{
					printf("SetInformationJobObject failed: error 0x%X\n", GetLastError());
					return;
				}
			}

			pi = std::make_shared< PROCESS_INFORMATION >(PROCESS_INFORMATION{ 0 });

			std::string OutputCommandline = stdfs::path(CorrectedProcessPath).filename().generic_string();
			std::string WorkingDir = stdfs::path(CorrectedProcessPath).parent_path().generic_string();

			OutputCommandline += " ";
			OutputCommandline += Commandline;

			STARTUPINFOA si = { 0 };
			si.cb = sizeof(si);
			if (bStartVisible == false)
			{
				si.dwFlags = STARTF_USESHOWWINDOW;
				si.wShowWindow = SW_HIDE;
			}

			if (bInputToString)
			{
				CreateReadHandle();

				si.hStdError = _childStd_OUT_Wr;
				si.hStdOutput = _childStd_OUT_Wr;
				si.dwFlags |= STARTF_USESTDHANDLES;
			}

			DWORD dwCreationFlags = 0;
			dwCreationFlags |= CREATE_NEW_CONSOLE;
			bSuccess = CreateProcessA(CorrectedProcessPath, (LPSTR)OutputCommandline.c_str(),
				NULL, NULL, bInputToString,
				dwCreationFlags, NULL, WorkingDir.c_str(), &si, pi.get());
			if (bSuccess == 0)
			{
				auto LastError = GetLastError();
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateProcess failed : error(%d)0x%X", LastError, LastError);
				return;
			}

			_processID = pi->dwProcessId;

			if (bInputToString)
			{
				_outputThread = std::thread(&Win32Process::ReadSTDThread, this);
							
				// Close handles to the stdin and stdout pipes no longer needed by the child process.
				// If they are not explicitly closed, there is no way to recognize that the child process has ended.
				CloseHandle(_childStd_OUT_Wr);				
			}

			// could be null if parent was alreayd in a job so child will auto inherit it
			if (hJob != nullptr)
			{
				bSuccess = AssignProcessToJobObject(hJob, pi->hProcess);
				if (bSuccess == 0)
				{
					SPP_LOG(LOG_WIN32CORE, LOG_INFO, "AssignProcessToJobObject failed: error %d", GetLastError());
					return;
				}
			}			
		}

		virtual ~Win32Process()
		{
			if(IsRunning())
			{
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "~Win32Process: process was still running!");
				TerminateProcess(pi->hProcess, 0);
			}

			if (_outputThread.joinable())
			{
				_outputThread.join();
			}

			if (pi)
			{
				// Close handles to the child process and its primary thread.
				// Some applications might keep these handles to monitor the status
				// of the child process, for example. 
				CloseHandle(pi->hProcess);
				CloseHandle(pi->hThread);
				pi = nullptr;
			}
		}

		void CreateReadHandle()
		{
			SECURITY_ATTRIBUTES saAttr;

			// Set the bInheritHandle flag so pipe handles are inherited. 
			saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
			saAttr.bInheritHandle = TRUE;
			saAttr.lpSecurityDescriptor = NULL;

			// Create a pipe for the child process's STDOUT. 
			if (!CreatePipe(&_childStd_OUT_Rd, &_childStd_OUT_Wr, &saAttr, 0))
			{ 
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateReadHandle: CreatePipe failed");
			}			
			if (!SetHandleInformation(_childStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
			{
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateReadHandle: SetHandleInformation failed");
			}
		}
				
		void ReadSTDThread()
		{
			CHAR chBuf[512];
			while (true)
			{
				DWORD dwRead = 0;
				auto bSuccess = ReadFile(_childStd_OUT_Rd, chBuf, 512, &dwRead, NULL);
				if (!bSuccess || dwRead == 0)
				{
					break;
				}
				else
				{
					std::unique_lock<std::mutex> lock(_outputMutex);
					_outputString += std::string(chBuf, chBuf + dwRead);
				}
			}
		}

		virtual bool IsValid()
		{
			return (pi && pi->dwProcessId);
		}

		virtual bool IsRunning()
		{
			if (pi)
			{
				DWORD exit_code;
				GetExitCodeProcess(pi->hProcess, &exit_code);
				if (exit_code == STILL_ACTIVE) 
				{
					return true;
				}
			}

			return false;
		}

		virtual std::string GetOutput() override
		{
			if (_outputThread.joinable())
			{ 
				_outputThread.join(); 
			}

			std::unique_lock<std::mutex> lock(_outputMutex);
			return _outputString;
		}
	};

	std::shared_ptr<PlatformProcess> CreatePlatformProcess(const char* ProcessPath, const char* Commandline, bool bStartVisible, bool bInPutToString)
	{
		return std::make_shared< Win32Process >(ProcessPath, Commandline, bStartVisible, bInPutToString);
	}

	std::map< uint32_t, std::shared_ptr< PROCESS_INFORMATION > > hostedChildProcesses;

	uint32_t CreateChildProcess(const char* _ProcessPath, const char* Commandline, bool bStartVisible)
	{
		std::string stringProcessPath = _ProcessPath;

		stdfs::path asPath(_ProcessPath);
		if (!asPath.has_extension())
		{
			stringProcessPath += ".exe";
		}
		const char* ProcessPath = stringProcessPath.c_str();

		SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: %s %s", ProcessPath, Commandline);

		BOOL bIsProcessInJob = false;
		BOOL bSuccess = IsProcessInJob(GetCurrentProcess(), NULL, &bIsProcessInJob);
		if (bSuccess == 0) 
		{
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: IsProcessInJob failed: error %d", GetLastError());
			return 0;
		}
		
		bool bCreateJob = true;

		HANDLE hJob = nullptr;
		if (bIsProcessInJob) 
		{
			bCreateJob = false;
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: already in job");

			JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
			QueryInformationJobObject(NULL, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli), NULL);

			SPP_LOG(LOG_WIN32CORE, LOG_INFO, " -  silent break away %d", (jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK));
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, " -  kill on close %d", (jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, " -  break away %d", (jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK));

			bCreateJob = (jeli.BasicLimitInformation.LimitFlags & (JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK)) != 0;

			SPP_LOG(LOG_WIN32CORE, LOG_INFO, " - bCreateJob %d", bCreateJob);
		}
		
		if(bCreateJob)
		{
			hJob = CreateJobObject(NULL, NULL);
			if (hJob == NULL) 
			{
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateJobObject failed : error % d", GetLastError());
				return 0;
			}

			JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
			jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
			bSuccess = SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
			if (bSuccess == 0) {
				printf("SetInformationJobObject failed: error %d\n", GetLastError());
				return 0;
			}
		}
	
		auto pi = std::make_shared< PROCESS_INFORMATION >(PROCESS_INFORMATION{ 0 });

		std::string OutputCommandline = stdfs::path(ProcessPath).filename().generic_string();
		std::string WorkingDir = stdfs::path(ProcessPath).parent_path().generic_string();

		OutputCommandline += " ";
		OutputCommandline += Commandline;

		STARTUPINFOA si = { 0 };
		si.cb = sizeof(si);
		if (bStartVisible == false)
		{
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
		}

		DWORD dwCreationFlags = 0;
		dwCreationFlags |= CREATE_NEW_CONSOLE;
		bSuccess = CreateProcessA(ProcessPath, (LPSTR)OutputCommandline.c_str(),
			NULL, NULL, FALSE,
			dwCreationFlags, NULL, WorkingDir.empty() ? nullptr : WorkingDir.c_str(), &si, pi.get());
		if (bSuccess == 0) 
		{
			auto LastError = GetLastError();
			SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateProcess failed : error 0x%X : %d", LastError, LastError);
			return 0;
		}

		// could be null if parent was alreayd in a job so child will auto inherit it
		if (hJob != nullptr)
		{
			bSuccess = AssignProcessToJobObject(hJob, pi->hProcess);
			if (bSuccess == 0) 
			{
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "AssignProcessToJobObject failed: error %d", GetLastError());
				return 0;
			}
		}

		hostedChildProcesses[pi->dwProcessId] = pi;

		return pi->dwProcessId;
	}


	bool IsChildRunning(uint32_t processID)
	{
		auto foundChild = hostedChildProcesses.find(processID);

		if (foundChild != hostedChildProcesses.end())
		{
			DWORD exit_code;
			GetExitCodeProcess(foundChild->second->hProcess, &exit_code);
			if (exit_code == STILL_ACTIVE) {
				return true;
			}
		}

		return false;
	}

	void ShowMouse(bool bShowMouse)
	{
		ShowCursor(false);
	}

	void CaptureWindow(void* Window)
	{
		if (Window)
		{
			SetCapture((HWND)Window);
		}
		else
		{
			ReleaseCapture();
		}
	}	

	void CloseChild(uint32_t processID)
	{
		auto foundChild = hostedChildProcesses.find(processID);

		if (foundChild != hostedChildProcesses.end())
		{
			//PostThreadMessage(foundChild->second->dwThreadId, WM_CLOSE, 0, 0);			
			TerminateProcess(foundChild->second->hProcess, 0);
		}
	}

	void AddDLLSearchPath(const char* InPath)
	{
		SetDllDirectoryA(InPath);
	}
}

uint32_t C_CreateChildProcess(const char* ProcessPath, const char* Commandline, bool bStartVisible)
{
	return SPP::CreateChildProcess(ProcessPath, Commandline, bStartVisible);
}

bool C_IsChildRunning(uint32_t processID)
{
	return SPP::IsChildRunning(processID);
}

void C_CloseChild(uint32_t processID)
{
	return SPP::CloseChild(processID);
}