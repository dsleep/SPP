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

#include "SPPProfiler.h"

#include "Windows.h"
#include "sysinfoapi.h"
#include "commdlg.h"

namespace SPP
{
	SPP_CORE_API LogEntry LOG_WIN32CORE("WIN32CORE");

	LONG GetDWORDRegKey(HKEY hKey, const std::string& strValueName, DWORD& nValue)
	{
		DWORD dwBufferSize(sizeof(DWORD));
		DWORD nResult(0);
		LONG nError = ::RegQueryValueExA(hKey,
			strValueName.c_str(),
			0,
			NULL,
			reinterpret_cast<LPBYTE>(&nResult),
			&dwBufferSize);
		if (ERROR_SUCCESS == nError)
		{
			nValue = nResult;
		}
		return nError;
	}

	uint32_t ProcSpeedRead()
	{
		DWORD BufSize = sizeof(BYTE);
		DWORD dwMHz = 0;
		HKEY hKey;

		// open the key where the proc speed is hidden:
		auto lError = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
			"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
			0,
			KEY_READ,
			&hKey);

		if (lError == ERROR_SUCCESS)
		{
			GetDWORDRegKey(hKey, "~MHz", dwMHz);
		}

		return dwMHz;
	}

	const ComputerInfo &GetComputerInfo()
	{
		static bool bHasValue = false;
		static ComputerInfo sO;

		if (!bHasValue)
		{
			MEMORYSTATUSEX statex;
			statex.dwLength = sizeof(statex);
			GlobalMemoryStatusEx(&statex);
			auto RamAmmount = statex.ullTotalPhys / (1024 * 1024);

			SYSTEM_INFO info;
			GetSystemInfo(&info);


			DWORD buffer_size = 0;
			GetLogicalProcessorInformation(0, &buffer_size);

			uint32_t PhysCoreCount = 0;
			{
				DWORD dwNum = buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
				SE_ASSERT(buffer_size == (dwNum * sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)));
				std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer;
				buffer.resize(dwNum);
				GetLogicalProcessorInformation(buffer.data(), &buffer_size);
				
				for (auto& curCPU : buffer)
				{
					if (curCPU.Relationship == RelationProcessorCore)
					{
						PhysCoreCount++;
					}
				}
			}

			sO = {
				.CPU_SpeedInMHz = ProcSpeedRead(),
				.CPU_LogicalCores = std::thread::hardware_concurrency(),
				.CPU_PhysicalCores = PhysCoreCount,
				.RAM_PageSize = info.dwPageSize,
				.RAM_InMBs = (uint32_t)(statex.ullTotalPhys / (1024 * 1024))
			};

			bHasValue = true;
		}

		return sO;
	}

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
		PROCESS_INFORMATION _pi = { 0 };

		std::mutex _outputMutex;
		std::string _outputString;

		bool _bIsChildProcess = true;

	public:

		Win32Process(const char* ProcessPath, const char* Commandline, bool bStartVisible, 
			bool bInputToString, bool bIsChildProcess) :
			PlatformProcess(ProcessPath, Commandline, bStartVisible, bInputToString), _bIsChildProcess(bIsChildProcess)
		{
			std::string stringProcessPath = _processPath;
			stdfs::path asPath(_processPath);
			if (!asPath.has_extension())
			{
				stringProcessPath += ".exe";
			}
			const char* CorrectedProcessPath = stringProcessPath.c_str();

			SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateProcess(isChild:%d): %s %s", _bIsChildProcess, CorrectedProcessPath, Commandline);

			HANDLE hJob = nullptr;
			BOOL bSuccess = TRUE;
			if (_bIsChildProcess)
			{
				BOOL bIsProcessInJob = false;
				bSuccess = IsProcessInJob(GetCurrentProcess(), NULL, &bIsProcessInJob);
				if (bSuccess == 0)
				{
					SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateChildProcess: IsProcessInJob failed: error %d", GetLastError());
					return;
				}

				bool bCreateJob = true;

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
			}

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
				dwCreationFlags, NULL, WorkingDir.c_str(), &si, &_pi);
			if (bSuccess == 0)
			{
				auto LastError = GetLastError();
				
				LPVOID lpMsgBuf;

				FormatMessageA(
					FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM |
					FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					LastError,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPSTR)&lpMsgBuf,
					0, NULL);

				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "CreateProcess failed : error(%d)0x%X : %s", LastError, LastError, (const char*)lpMsgBuf);

				LocalFree(lpMsgBuf);

				return;
			}

			_processID = _pi.dwProcessId;

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
				bSuccess = AssignProcessToJobObject(hJob, _pi.hProcess);
				if (bSuccess == 0)
				{
					SPP_LOG(LOG_WIN32CORE, LOG_INFO, "AssignProcessToJobObject failed: error %d", GetLastError());
					return;
				}
			}			
		}

		virtual ~Win32Process()
		{
			if(_bIsChildProcess && IsRunning())
			{
				SPP_LOG(LOG_WIN32CORE, LOG_INFO, "~Win32Process: process was still running!");
				TerminateProcess(_pi.hProcess, 0);
			}

			if (_outputThread.joinable())
			{
				_outputThread.join();
			}

			if (_pi.hProcess)
			{
				CloseHandle(_pi.hProcess);
			}
			if (_pi.hThread)
			{
				CloseHandle(_pi.hThread);
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
			return (_pi.dwProcessId);
		}

		virtual bool IsRunning()
		{
			if (_pi.hProcess)
			{
				DWORD exit_code;
				GetExitCodeProcess(_pi.hProcess, &exit_code);
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

	std::shared_ptr<PlatformProcess> CreatePlatformProcess(const char* ProcessPath, const char* Commandline, bool bStartVisible, bool bInPutToString, bool bAsChildProcess)
	{
		return std::make_shared< Win32Process >(ProcessPath, Commandline, bStartVisible, bInPutToString, bAsChildProcess);
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
		static bool bDirectoriesSet = false;
		if (!bDirectoriesSet)
		{
			SE_ASSERT(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
				LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
				LOAD_LIBRARY_SEARCH_SYSTEM32 |
				LOAD_LIBRARY_SEARCH_USER_DIRS) != 0);
			bDirectoriesSet = true;
		}

		char path[MAX_PATH] = { 0 };

		DWORD result = GetModuleFileNameA(nullptr, path, MAX_PATH);

		stdfs::path BinaryPath = path;

		std::string PathAsString = stdfs::absolute(BinaryPath / "../" / InPath).make_preferred().generic_string();

		auto wstring = std::utf8_to_wstring(PathAsString);
		SE_ASSERT(AddDllDirectory(wstring.c_str()) != 0);
	}

	void inline_forward_replace(std::string& iopath) {
		std::replace(iopath.begin(), iopath.end(), '/', '\\');
	}

	std::string FileOpenDialog(const std::string &StartingPath, const std::vector<std::string>& Exts)
	{
		char szFilter[MAX_PATH];
		ZeroMemory(szFilter, MAX_PATH);

		auto filterIdx = szFilter;

		strcpy_s(filterIdx, MAX_PATH - (filterIdx - szFilter), "All (*.*)");
		filterIdx += 10;
		strcpy_s(filterIdx, MAX_PATH - (filterIdx - szFilter), "*.*");
		filterIdx += 4;

		// must be in pairs
		SE_ASSERT((Exts.size() % 2) == 0);
		for (auto& curExt : Exts)
		{
			strcpy_s(filterIdx, MAX_PATH - (filterIdx - szFilter), curExt.c_str());
			filterIdx += curExt.size() + 1;
		}

		// terminate it
		strcpy_s(filterIdx, MAX_PATH - (filterIdx - szFilter), "\0\0");

		auto fullPath = stdfs::path(StartingPath).make_preferred();

		std::string InitialDirectory;

		char szFile[MAX_PATH];
		ZeroMemory(szFile, MAX_PATH);

		if (!fullPath.empty())
		{
			if (fullPath.has_filename() && fullPath.has_extension())
			{
				strcpy_s(szFile, MAX_PATH, fullPath.filename().generic_string().c_str());

				InitialDirectory = fullPath.parent_path().generic_string();
			}
			else
			{
				InitialDirectory = fullPath.generic_string();
			}
		}

		inline_forward_replace(InitialDirectory);

		OPENFILENAMEA ofn;       // common dialog box structure
		// Initialize OPENFILENAME
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = nullptr;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = szFilter;
		ofn.nFilterIndex = 0;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = InitialDirectory.empty() ? nullptr : InitialDirectory.c_str();
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameA(&ofn))
		{
			// The file was loaded successfully.
			return szFile;
		}
		else
		{
			// The file was not loaded successfully.
			return "";
		}
	}

	std::string FileSaveDialog(const std::string& StartingPath, const std::vector<std::string> &Exts)
	{
		char szFilter[MAX_PATH];
		ZeroMemory(szFilter, MAX_PATH);

		auto filterIdx = szFilter;

		strcpy_s(filterIdx, MAX_PATH - (filterIdx - szFilter), "All (*.*)");
		filterIdx += 10;
		strcpy_s(filterIdx, MAX_PATH - (filterIdx - szFilter), "*.*");
		filterIdx += 4;

		// must be in pairs
		SE_ASSERT((Exts.size() % 2) == 0);
		for (auto& curExt : Exts)
		{
			strcpy_s(filterIdx, MAX_PATH - (filterIdx - szFilter), curExt.c_str());
			filterIdx += curExt.size() + 1;
		}

		// terminate it
		strcpy_s(filterIdx, MAX_PATH - (filterIdx - szFilter), "\0\0");

		auto fullPath = stdfs::path(StartingPath).make_preferred();

		std::string InitialDirectory;

		char szFile[MAX_PATH];
		ZeroMemory(szFile, MAX_PATH);

		if (!fullPath.empty())
		{
			if (fullPath.has_filename() && fullPath.has_extension())
			{
				strcpy_s(szFile, MAX_PATH, fullPath.filename().generic_string().c_str());

				InitialDirectory = fullPath.parent_path().generic_string();
			}
			else
			{
				InitialDirectory = fullPath.generic_string();
			}
		}

		inline_forward_replace(InitialDirectory);

		OPENFILENAMEA ofn;       // common dialog box structure
		// Initialize OPENFILENAME
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = nullptr;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = szFilter;
		ofn.nFilterIndex = 0;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = InitialDirectory.empty() ? nullptr : InitialDirectory.c_str();
		ofn.Flags = OFN_PATHMUSTEXIST;

		if (GetSaveFileNameA(&ofn))
		{
			// The file was loaded successfully.
			return szFile;
		}
		else
		{
			// The file was not loaded successfully.
			return "";
		}
	}

	bool IsRemoteDesktop()
	{
		return GetSystemMetrics(SM_REMOTESESSION);
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

