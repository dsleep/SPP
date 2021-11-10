// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPPlatformCore.h"
#include "SPPString.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"

#include <unistd.h>

#include <spawn.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <map>
#include <thread>

namespace SPP
{
	SPP_CORE_API LogEntry LOG_MACOSCORE("MACOSCORE");

	std::string GetProcessName()
	{
        const char * appname = getprogname();
		return appname;
	}

	PlatformInfo GetPlatformInfo()	
	{

        PlatformInfo oInfo = { (uint32_t) getpagesize(), std::thread::hardware_concurrency() };
		return oInfo;
	}

	SPP_CORE_API void GetMonitorInfo(std::vector<MonitorInfo>& oInfos)
	{
        
	}

    struct ProcessInformation
    {
        std::string Path;
        std::string Arguments;
    };

    
    std::mutex childMutex;
    std::map< uint32_t, std::shared_ptr< ProcessInformation > > hostedChildProcesses;

    void child_sig(int signo)
    {
        pid_t pid;
        int   status;
        while ((pid = waitpid(-1, &status, WNOHANG)) != -1)
        {
            //
            //unregister_child(pid, status);   // Or whatever you need to do with the PID
        }
    }

	uint32_t CreateChildProcess(const char* ProcessPath, const char* Commandline, bool bStartVisible)
	{
		SPP_LOG(LOG_MACOSCORE, LOG_INFO, "CreateChildProcess: %s %s", ProcessPath, Commandline);
        
        static bool bCreatedSig = false;
        if(bCreatedSig == false)
        {
            signal (SIGCHLD, child_sig);
            bCreatedSig = true;
        }
        
        std::string ProcessCleaned = stdfs::path(ProcessPath).filename().generic_string();
        
        pid_t pid { 0 };
        const char *argv[] = {ProcessCleaned.c_str(), Commandline, NULL};
        const char *environ[] = {NULL};
        auto status = posix_spawn(&pid, ProcessPath, NULL, NULL, (char *const*)argv, (char *const*)environ);
        
        if (status == 0)
        {
            printf("Child pid: %i\n", pid);
        }
        else
        {
            printf("posix_spawn: %s\n", strerror(status));
            return 0;
        }
        
        std::shared_ptr<ProcessInformation> newChild;
        newChild.reset( new ProcessInformation{ProcessPath, Commandline} );
        
        {
            std::unique_lock<std::mutex> lk(childMutex);
            hostedChildProcesses[(uint32_t)pid] = newChild;
        }
        
        return (uint32_t)pid;
	}

	bool IsChildRunning(uint32_t processID)
	{
		auto foundChild = hostedChildProcesses.find(processID);

		if (foundChild != hostedChildProcesses.end())
		{
            int status = 0;
            pid_t result = waitpid(processID, &status, WNOHANG);
            if (result == 0) {
              // Child still alive
            } else if (result == -1) {
              // Error
            } else {
              // Child exited
            }
		}

		return false;
	}

	void ShowMouse(bool bShowMouse)
	{
		
	}

	void CaptureWindow(void* Window)
	{
		
	}	

	void CloseChild(uint32_t processID)
	{
        auto foundChild = hostedChildProcesses.find(processID);
        if (foundChild != hostedChildProcesses.end())
        {
            kill(processID, SIGTERM);
        }
	}

	void AddDLLSearchPath(const char* InPath)
	{
        
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
