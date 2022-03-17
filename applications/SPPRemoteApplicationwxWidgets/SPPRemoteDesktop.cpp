// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


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

#define MAX_LOADSTRING 100

SPP_OVERLOAD_ALLOCATORS

using namespace std::chrono_literals;
using namespace SPP;

void JSFunctionReceiver(const std::string& InFunc, Json::Value& InValue)
{
	/*auto EditorType = rttr::type::get<EditorEngine>();

	rttr::method foundMethod = EditorType.get_method(InFunc);
	if (foundMethod)
	{
		auto paramInfos = foundMethod.get_parameter_infos();

		uint32_t jsonParamCount = InValue.isNull() ? 0 : InValue.size();

		if (paramInfos.size() == jsonParamCount)
		{
			if (!jsonParamCount)
			{
				foundMethod.invoke(*GEd);
			}
			else
			{
				std::list<rttr::variant> argRefs;
				std::vector<rttr::argument> args;

				int32_t Iter = 0;
				for (auto& curParam : paramInfos)
				{
					auto curParamType = curParam.get_type();
					auto jsonParamValue = InValue[Iter];
					if (curParamType.is_arithmetic() && jsonParamValue.isNumeric())
					{

					}
					else if (curParamType == rttr::type::get<std::string>() &&
						jsonParamValue.isString())
					{
						argRefs.push_back(std::string(jsonParamValue.asCString()));
						args.push_back(argRefs.back());
					}
					Iter++;
				}

				if (args.size() == paramInfos.size())
				{
					foundMethod.invoke_variadic(*GEd, args);
				}
			}
		}
	}*/
}

///////////////////////////////////////
//
///////////////////////////////////////

struct HostFromCoord
{
	std::string NAME;
	std::string APPNAME;
	std::string APPCL;
	std::string GUID;
};

uint32_t GProcessID = 0;
std::string GIPCMemoryID;
std::unique_ptr< std::thread > GWorkerThread;
std::unique_ptr< IPCMappedMemory > GIPCMem;
const int32_t MemSize = 2 * 1024 * 1024;

void WorkerThread()
{
	GIPCMemoryID = std::generate_hex(3);
	GIPCMem.reset(new IPCMappedMemory(GIPCMemoryID.c_str(), MemSize, true));

	std::string ArgString = std::string_format("-MEM=%s", GIPCMemoryID.c_str());

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
					auto hasCoord = outRoot["COORD"].asUInt();
					auto resolvedStun = outRoot["RESOLVEDSDP"].asUInt();
					auto btConn = outRoot["BLUETOOTH"].asUInt();
					auto conncetionStatus = outRoot["CONNSTATUS"].asUInt();

					static int32_t hasCoordV = 0;
					static int32_t resolvedStunV = 0;

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
	}
}



int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

	//Alloc Console
	//print some stuff to the console
	//make sure to include #include "stdio.h"
	//note, you must use the #include <iostream>/ using namespace std
	//to use the iostream... #incldue "iostream.h" didn't seem to work
	//in my VC 6
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
	printf("Debugging Window:\n");

	SPP::IntializeCore(std::wstring_to_utf8(lpCmdLine).c_str());

#if PLATFORM_WINDOWS
	_CrtSetDbgFlag(0);
#endif

	// setup global asset path
	SPP::GAssetPath = stdfs::absolute(stdfs::current_path() / "..\\Assets\\").generic_string();

	// start thread
	GWorkerThread.reset(new std::thread(WorkerThread));

	{
		std::function<void(const std::string&, Json::Value&) > jsFuncRecv = JSFunctionReceiver;

		std::thread runCEF([hInstance, &jsFuncRecv]()
		{
			SPP::RunBrowser(hInstance,
				"http://spp/assets/web/remotedesktop/index.html",
				{
					nullptr,
					nullptr,
					nullptr,
					nullptr,
				},
				{
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					nullptr,
				},
				& jsFuncRecv);
		});

		runCEF.join();

		//SHUTDOWN OUR APP
		StopThread();
	}

	return 0;
}


