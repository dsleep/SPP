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

#include "SPPHandledTimers.h"
#include "SPPMemory.h"
#include "SPPReflection.h"

#include "ThreadPool.h"
#include "SPPCEFUI.h"
#include "SPPJsonUtils.h"

#include "cefclient/JSCallbackInterface.h"
#include <condition_variable>

#include "SPPPlatformCore.h"

#include "SPPNetworkConnection.h"
#include "SPPNetworkMessenger.h"
#include "SPPNatTraversal.h"

SPP_OVERLOAD_ALLOCATORS

using namespace std::chrono_literals;
using namespace SPP;

#include <shellapi.h>


LogEntry LOG_RD("RemoteDesktop");

std::unique_ptr< std::thread > GMainThread;
std::unique_ptr< std::thread > GWorkerThread;
uint32_t GProcessID = 0;
std::string GIPCMemoryID;
std::unique_ptr< IPCMappedMemory > GIPCMem;
const int32_t MemSize = 2 * 1024 * 1024;


void UpdateConfig(const std::string& InValue)
{


}

void HelpClick(const std::string &SubType)
{
	if (SubType == "GIT")
	{
		ShellExecuteA(NULL, "open", "https://github.com/dsleep/SPP", NULL, NULL, SW_SHOWNORMAL);
	}
	else if (SubType == "About")
	{
		ShellExecuteA(NULL, "open", "https://github.com/dsleep/SPP", NULL, NULL, SW_SHOWNORMAL);
	}
}



template<typename... Args>
void CallCPPReflected(const std::string &InMethod, const Args&... InArgs)
{
	using namespace rttr;

	method meth = type::get_global_method(InMethod.c_str());
	if (meth) // check if the function was found
	{
		//meth.get_parameter_infos()
		//return_value = meth.invoke({}, 9.0, 3.0); // invoke with empty instance
		//if (return_value.is_valid() && return_value.is_type<double>())
		//	std::cout << return_value.get_value<double>() << std::endl; // 729
	}
}




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
	
	using namespace rttr;

	method meth = type::get_global_method(InFunc.c_str());
	if (meth) // check if the function was found
	{
		auto paramInfos = meth.get_parameter_infos();

		if (jsonParamCount == paramInfos.size())
		{
			std::vector< variant > variants;
			variants.reserve(paramInfos.size());
			std::vector< argument > arguments;
			arguments.reserve(paramInfos.size());

			uint32_t Iter = 0;
			for (const auto& info : paramInfos)
			{
				auto curValue = InValue[Iter];
				auto curType = info.get_type();

				if (curType.is_arithmetic() ||
					curType.is_enumeration() ||
					curType == rttr::type::get<std::string>())
				{										
					variant stringValue = (std::string)curValue.asString();

					if (stringValue.can_convert(curType))
					{
						stringValue.convert((const type)curType);
						variants.emplace_back(std::move(stringValue));

						arguments.push_back(variants.back());
					}
					else
					{
						SPP_LOG(LOG_RD, LOG_INFO, "Invoke Native From JS Failed, no conversion");					
						return;
					}
				}

				Iter++;
			}

			auto results = meth.invoke_variadic({}, arguments);
			if (results.is_valid() == false)
			{
				SPP_LOG(LOG_RD, LOG_INFO, "Invoke Native From JS Failed");
			}
		}
	}

	if (InFunc == "ButtonClick" && jsonParamCount == 1)
	{
		
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

struct LANConfiguration
{
	uint16_t port=12;
};

struct CoordinatorConfiguration
{
	std::string addr = "estest";
	std::string pwd = "asdfasdf";
};

struct STUNConfiguration
{
	std::string addr = "stun.l.google.com";
	uint16_t port = 19302;
};

struct APPConfig
{
	LANConfiguration lan;
	CoordinatorConfiguration coord;
	STUNConfiguration stun;
};


APPConfig GAppConfig;

struct RemoteClient
{
	std::chrono::steady_clock::time_point LastUpdate;
	std::string Name;
	std::string AppName;
	std::string AppCL;
};

void LoadConfigs()
{
	auto coordRef = std::ref(GAppConfig);

	Json::Value jsonData;
	FileToJson("./remotedesktop.config.txt", jsonData);
	//
	// void JSONToPOD(const rttr::instance& inValue, const Json::Value& InJsonValue)s
	//remotedesktop.config.txt
}

void PageLoaded()
{
	auto coordRef = std::ref(GAppConfig);

	GAppConfig.lan.port = 69;
	GAppConfig.coord.addr = "dave";

	Json::Value jsonData;
	PODToJSON(coordRef, jsonData);

	JsonToFile("./remotedesktop.config.txt", jsonData);

	std::string oString;
	JsonToString(jsonData, oString);
	JavascriptInterface::InvokeJS("SetConfig", "CoordConfig", oString);
}

RTTR_REGISTRATION
{
	using namespace rttr;
	registration::method("UpdateConfig", &UpdateConfig)
		.method("PageLoaded", &PageLoaded)
		.method("HelpClick", &HelpClick);

	rttr::registration::class_<LANConfiguration>("LANConfiguration")
		.property("port", &LANConfiguration::port)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<CoordinatorConfiguration>("CoordinatorConfiguration")
		.property("addr", &CoordinatorConfiguration::addr)(rttr::policy::prop::as_reference_wrapper)
		.property("pwd", &CoordinatorConfiguration::pwd)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<STUNConfiguration>("STUNConfiguration")
		.property("addr", &STUNConfiguration::addr)(rttr::policy::prop::as_reference_wrapper)
		.property("port", &STUNConfiguration::port)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<APPConfig>("APPConfig")
		.property("lan", &APPConfig::lan)(rttr::policy::prop::as_reference_wrapper)
		.property("coord", &APPConfig::coord)(rttr::policy::prop::as_reference_wrapper)
		.property("stun", &APPConfig::stun)(rttr::policy::prop::as_reference_wrapper)
		;
}


void MainThread()
{
	auto ThisRUNGUID = std::generate_hex(3);

	std::map<std::string, RemoteClient> Hosts;

	//std::unique_ptr<UDP_SQL_Coordinator> coordinator = std::make_unique<UDP_SQL_Coordinator>(GAppConfig.coord.addr);
	std::unique_ptr<UDPSocket> broadReceiver = std::make_unique<UDPSocket>(GAppConfig.lan.port, UDPSocketOptions::Broadcast);
	std::unique_ptr<UDPJuiceSocket> juiceSocket = std::make_unique<UDPJuiceSocket>(GAppConfig.stun.addr.c_str(), GAppConfig.stun.port);

	//coordinator->SetPassword(GAppConfig.coord.pwd);
	//coordinator->SetKeyPair("GUID", ThisRUNGUID);
	//coordinator->SetKeyPair("NAME", GetOSNetwork().HostName);
	//coordinator->SetKeyPair("LASTUPDATETIME", "datetime('now')");

	//CHECK BROADCASTS
	std::vector<uint8_t> BufferRead;
	BufferRead.resize(std::numeric_limits<uint16_t>::max());

	TimerController mainController(16ms);

	mainController.AddTimer(100ms, true, [&]()
		{
			IPv4_SocketAddress recvAddr;
			int32_t DataRecv = 0;
			while ((DataRecv = broadReceiver->ReceiveFrom(recvAddr, BufferRead.data(), BufferRead.size())) > 0)
			{
				//SPP_LOG(LOG_APP, LOG_INFO, "UDP BROADCAST!!!");
				std::string HostString((char*)BufferRead.data(), (char*)BufferRead.data() + DataRecv);
				IPv4_SocketAddress hostPort(HostString.c_str());

				recvAddr.Port = hostPort.Port;
				auto realAddrOfConnection = recvAddr.ToString();

				Hosts[realAddrOfConnection] = RemoteClient{
					std::chrono::steady_clock::now(),
					realAddrOfConnection,
					std::string(""),
					std::string("")
				};
			}
		});

	//ICE/STUN management
	mainController.AddTimer(100ms, true, [&]()
		{
			if (juiceSocket->IsReady())
			{
				//coordinator->SetKeyPair("SDP", std::string(juiceSocket->GetSDP_BASE64()));
			}
			//if (juiceSocket->HasProblem())
			//{
			//	juiceSocket = std::make_unique<UDPJuiceSocket>(StunURL.c_str(), StunPort);
			//	SPP_LOG(LOG_APP, LOG_INFO, "Resetting juice socket from problem (error on join usually)");
			//}
			//else if (juiceSocket->IsConnected())
			//{

			//}
		});

	mainController.Run();
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
		true);

#if _DEBUG
	if (!bLanOnly)
	{
		CreateChildProcess("simpleconnectioncoordinatord", "", true);
	}
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

		ArgString.c_str(), true);

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
	if (GMainThread)
	{
		if (GMainThread->joinable())
		{
			GMainThread->join();
		}
		GMainThread.reset();		
	}

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
	
	GMainThread.reset(new std::thread(MainThread));

	{
		std::function<void(const std::string&, Json::Value&) > jsFuncRecv = JSFunctionReceiver;

		std::thread runCEF([hInstance, &jsFuncRecv]()
		{
			SPP::RunBrowser(hInstance, "http://spp/assets/web/remotedesktop/indexV2.html", {}, {}, false, &jsFuncRecv);
		});

		runCEF.join();

		//SHUTDOWN OUR APP
		StopThread();
	}

	return 0;
}


