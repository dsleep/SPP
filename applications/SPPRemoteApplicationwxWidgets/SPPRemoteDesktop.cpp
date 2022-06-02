// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

// Currently PC only!!!


// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <memory>
#include <thread>
#include <optional>
#include <sstream>
#include <set>

#include "SPPString.h"
#include "SPPEngine.h"
#include "SPPApplication.h"
#include "SPPSerialization.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"

#include "SPPMemory.h"
#include "SPPReflection.h"

#include "ThreadPool.h"
#include "SPPCEFUI.h"
#include "SPPJsonUtils.h"

#include "cefclient/JSCallbackInterface.h"
#include <condition_variable>

#include "SPPPlatformCore.h"

SPP_OVERLOAD_ALLOCATORS

using namespace std::chrono_literals;
using namespace SPP;

#include <shellapi.h>

std::unique_ptr< std::thread > GWorkerThread;
uint32_t GProcessID = 0;
std::string GIPCMemoryID;
std::unique_ptr< IPCMappedMemory > GIPCMem;
const int32_t MemSize = 2 * 1024 * 1024;

/// <summary>
/// Handles the functionality for all javascript->native calls.
/// 
/// In JS call native via window.CallNativeWithJSON 
/// </summary>
/// <param name="InFunc"></param>
/// <param name="InValue"></param>
void JSFunctionReceiver(const std::string& InFunc, Json::Value& InValue)
{
	uint32_t jsonParamCount = InValue.isNull() ? 0 : InValue.size();

	if (InFunc == "ButtonClick" && jsonParamCount == 1)
	{
		auto jsonParamValue = InValue[0];
		std::string SubCommandName = jsonParamValue.asCString();

		if (SubCommandName == "GIT")
		{
			ShellExecuteA(NULL, "open", "https://github.com/dsleep/SPP", NULL, NULL, SW_SHOWNORMAL);
		}
		else if (SubCommandName == "About")
		{
			ShellExecuteA(NULL, "open", "https://github.com/dsleep/SPP", NULL, NULL, SW_SHOWNORMAL);
		}
	}

	if (InFunc == "JoinHost" && jsonParamCount == 1)
	{
		auto jsonParamValue = InValue[0];
		std::string HostGUID = jsonParamValue.asCString();

		uint8_t hasData = ~(uint8_t)0;

		BinaryBlobSerializer outData;
		outData << hasData;
		outData << HostGUID;
		outData << std::string("");
		GIPCMem->WriteMemory(outData.GetData(), outData.Size(), 1 * 1024 * 1024);
	}

	if (InFunc == "ResetState")
	{
		extern void StopThread();
		StopThread();
	}
	else if (InFunc == "StartupHost")
	{
		auto jsonParamValue = InValue[0];
		bool IsLanOnly = (bool)jsonParamValue.asInt();

		extern void HostThread(bool);
		if (!GWorkerThread)GWorkerThread.reset(new std::thread(HostThread, IsLanOnly));
	}
	else if (InFunc == "StartupClient")
	{
		auto jsonParamValue = InValue[0]; 
		bool IsLanOnly = (bool)jsonParamValue.asInt();

		extern void ClientThread(bool);
		if (!GWorkerThread)GWorkerThread.reset(new std::thread(ClientThread, IsLanOnly));
	}

}

/// <summary>
/// Generic Status update coming from the child launched app "client" or "host"
/// </summary>
/// <param name="InJSON"></param>
void UpdateStatus(Json::Value &InJSON)
{
	auto hasCoord = InJSON["COORD"].asUInt();
	auto resolvedStun = InJSON["RESOLVEDSDP"].asUInt();
	auto connectionStatus = InJSON["CONNSTATUS"].asUInt();

	static int32_t hasCoordV = 0;
	static int32_t resolvedStunV = 0;
	static int32_t connectionStatusV = 0;

	if (hasCoord != hasCoordV)
	{
		hasCoordV = hasCoord;
		JavascriptInterface::CallJS("UpdateCoord", hasCoordV);
	}
	if (resolvedStun != resolvedStunV)
	{
		resolvedStunV = resolvedStun;
		JavascriptInterface::CallJS("UpdateSTUN", resolvedStunV);
	}
	if (connectionStatus != connectionStatusV)
	{
		connectionStatusV = connectionStatus;
		JavascriptInterface::CallJS("UpdateConnectionStatus", connectionStatusV);
	}
	if (connectionStatusV == 2)
	{
		Json::Value connectionName = InJSON.get("CONNNAME", Json::Value::nullSingleton());
		if (!connectionName.isNull())
		{
			auto connectionNameValue = connectionName.asCString();
			auto KBInValue = InJSON["KBIN"].asFloat();
			auto KBOutValue = InJSON["KBOUT"].asFloat();

			JavascriptInterface::CallJS("UpdateConnectionStats",
				std::string(connectionNameValue),
				KBInValue,
				KBOutValue);
		}
	}
}

/// <summary>
/// Thread started up if you click host....
/// </summary>
void HostThread(bool bLanOnly)
{
	GIPCMemoryID = std::generate_hex(3);
	std::string ArgString = std::string_format("-MEM=%s %s",
		GIPCMemoryID.c_str(), bLanOnly ? "-lanonly" : "");
	GIPCMem.reset(new IPCMappedMemory(GIPCMemoryID.c_str(), MemSize, true));

#if _DEBUG
	GProcessID = CreateChildProcess("applicationhostd",
#else
	GProcessID = CreateChildProcess("applicationhost",
#endif
		ArgString.c_str(),
		false);

#if _DEBUG
	CreateChildProcess("simpleconnectioncoordinatord", "", true);
#endif

	while (true)
	{
		// do stuff...
		if (!IsChildRunning(GProcessID))
		{			
			break;
		}
		else
		{
			auto memAccess = GIPCMem->Lock();

			MemoryView inMem(memAccess, MemSize);
			uint32_t dataSize = 0;
			inMem >> dataSize;
			if (dataSize)
			{
				inMem.RebuildViewFromCurrent();
				Json::Value outRoot;
				if (MemoryToJson(inMem.GetData(), dataSize, outRoot))
				{
					UpdateStatus(outRoot);
				}
			}

			*(uint32_t*)memAccess = 0;
			GIPCMem->Release();
			std::this_thread::sleep_for(250ms);
		}
	}
}


/// <summary>
/// Thread started up if you click connect to host
/// </summary>
void ClientThread(bool bLanOnly)
{
	GIPCMemoryID = std::generate_hex(3);
	GIPCMem.reset(new IPCMappedMemory(GIPCMemoryID.c_str(), MemSize, true));

	std::string ArgString = std::string_format("-MEM=%s %s",
		GIPCMemoryID.c_str(), bLanOnly ? "-lanonly" : "");

#if _DEBUG
	GProcessID = CreateChildProcess("remoteviewerd",
#else
	GProcessID = CreateChildProcess("remoteviewer",
#endif

		ArgString.c_str(), false);

	while (true)
	{
		// do stuff...
		if (!IsChildRunning(GProcessID))
		{
			//CHILD ISN'T RUNNING
			
			break;
		}
		else
		{
			auto memAccess = GIPCMem->Lock();

			MemoryView inMem(memAccess, MemSize);
			uint32_t dataSize = 0;
			inMem >> dataSize;
			if (dataSize)
			{
				inMem.RebuildViewFromCurrent();
				Json::Value outRoot;
				if (MemoryToJson(inMem.GetData(), dataSize, outRoot))
				{
					UpdateStatus(outRoot);

					Json::Value hostList = outRoot.get("HOSTS", Json::Value::nullSingleton());
					if (!hostList.isNull() && hostList.isArray())
					{
						static std::map< std::string, std::string > ActiveHosts;

						std::map< std::string, std::string > NewlySetHosts;
						for (int32_t Iter = 0; Iter < hostList.size(); Iter++)
						{
							auto currentHost = hostList[Iter];

							std::string HostName = currentHost["NAME"].asCString();
							std::string HostGUID = currentHost["GUID"].asCString();

							NewlySetHosts[HostGUID] = HostName;
						}

						for (auto& [key, value] : NewlySetHosts)
						{
							auto foundHost = ActiveHosts.find(key);

							if (foundHost == ActiveHosts.end())
							{
								JavascriptInterface::CallJS("AddHost", key, value);
								ActiveHosts[key] = value;
							}
						}

						if (NewlySetHosts.size() != ActiveHosts.size())
						{
							for (auto Iter = ActiveHosts.begin(); Iter != ActiveHosts.end();)
							{
								auto foundHost = NewlySetHosts.find(Iter->first);
								if (foundHost == ActiveHosts.end())
								{
									JavascriptInterface::CallJS("RemoveHost", Iter->first);
									Iter = ActiveHosts.erase(Iter);									
								}
								else
								{
									Iter++;
								}
							}
						}

					}
				}
			}

			*(uint32_t*)memAccess = 0;
			GIPCMem->Release();
			std::this_thread::sleep_for(250ms);
		}
	}
}

/// <summary>
/// Kill child works (host or client)
/// </summary>
void StopThread()
{
	if (GWorkerThread)
	{
		CloseChild(GProcessID);
		if (GWorkerThread->joinable())
		{
			GWorkerThread->join();
		}
		GWorkerThread.reset();
		GIPCMem.reset();
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

	//Alloc Console regardless of being on windows
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
	printf("Debugging Window:\n");

	// initialize our SPP core (logging, exceptions, ....)
	SPP::IntializeCore(std::wstring_to_utf8(lpCmdLine).c_str());

#if PLATFORM_WINDOWS
	_CrtSetDbgFlag(0);
#endif

	// setup global asset path
	SPP::GAssetPath = stdfs::absolute(stdfs::current_path() / "..\\Assets\\").generic_string();
	
	{
		std::function<void(const std::string&, Json::Value&) > jsFuncRecv = JSFunctionReceiver;

		std::thread runCEF([hInstance, &jsFuncRecv]()
		{
			SPP::RunBrowser(hInstance, "http://spp/assets/web/remotedesktop/index.html", {}, {}, &jsFuncRecv);
		});

		runCEF.join();

		//SHUTDOWN OUR APP
		StopThread();
	}

	return 0;
}


