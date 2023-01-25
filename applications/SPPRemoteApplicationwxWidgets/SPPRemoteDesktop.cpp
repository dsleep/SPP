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

std::unique_ptr< std::thread > GWorkerThread;
uint32_t GProcessID = 0;
std::string GIPCMemoryID;
std::unique_ptr< IPCMappedMemory > GIPCMem;
const int32_t MemSize = 2 * 1024 * 1024;



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

#include "SPP_RD_AppConfig.inl"

APPConfig GAppConfig;

struct RemoteClient
{
	std::chrono::steady_clock::time_point LastUpdate;
	std::string Name;
	std::string AppName;
	std::string AppCL;

	bool operator == (const RemoteClient& cmp) const
	{
		return Name == cmp.Name && 
			AppName == cmp.AppName &&
			AppCL == cmp.AppCL;
	}
};

#if _DEBUG
	static const char* REMOTE_ACCESS_APP = "applicationhostd";
#else
	static const char* REMOTE_ACCESS_APP = "applicationhost";
#endif

class MainThreadApp
{
private:
	std::unique_ptr< TimerController > _timer;
	std::unique_ptr< ThreadPool > _localThreadPool;
	std::unique_ptr< std::thread > _thread;

	std::thread::id _runThreadID;

	std::unique_ptr<UDP_SQL_Coordinator> _coordinator; 
	std::unique_ptr<UDPSocket> _broadReceiver;
	std::unique_ptr<UDPJuiceSocket> _juiceSocket;

	std::unique_ptr < IPCMappedMemory> _appIPC; 

	const uint8_t CoordID = 0;
	const uint8_t JuiceeID = 1;
	const uint8_t RemoteID = 2;
	bool _bLastSent[3] = { false,false,false };

	std::string _ThisRUNGUID;

	std::map<std::string, RemoteClient> _remoteDevices;

public:
	MainThreadApp()
	{
		_ThisRUNGUID = std::generate_hex(3);

		_timer = std::make_unique< TimerController >(16ms);
		_localThreadPool = std::make_unique<ThreadPool>("MainPool", 0);

		_thread.reset(new std::thread(&MainThreadApp::Run, this));
	}
	
	void Shutdown()
	{
		_timer->Stop();
		if (_thread->joinable())
		{
			_thread->join();
		}
		_thread.reset();
	}

	void UpdateConfig(const APPConfig& InConfig)
	{
		if (_runThreadID != std::this_thread::get_id())
		{
			_localThreadPool->enqueue([CpyConfig = InConfig, this]()
			{
				UpdateConfig(CpyConfig);
			});
			return;
		}

		if (InConfig.coord != GAppConfig.coord)
		{
			GAppConfig.coord = InConfig.coord;
			CreateCoordinator();
		}
		if (InConfig.lan != GAppConfig.lan)
		{
			GAppConfig.lan = InConfig.lan;
			_broadReceiver = std::make_unique<UDPSocket>(GAppConfig.lan.port, UDPSocketOptions::Broadcast);
		}
		if (InConfig.stun != GAppConfig.stun)
		{
			GAppConfig.stun = InConfig.stun;
			_juiceSocket = std::make_unique<UDPJuiceSocket>(GAppConfig.stun.addr.c_str(), GAppConfig.stun.port);
		}
		if (InConfig.remote != GAppConfig.remote)
		{
			GAppConfig.remote = InConfig.remote;
		
			if (GAppConfig.remote.bEnabled)
			{
				StartRemoteAccess();
			}
			else
			{
				CloseRemoteAccess();
			}
		}
	}

	void CreateCoordinator()
	{
		_coordinator = std::make_unique<UDP_SQL_Coordinator>(IPv4_SocketAddress(GAppConfig.coord.addr.c_str()));

		_coordinator->SetPassword(GAppConfig.coord.pwd);
		_coordinator->SetKeyPair("GUID", _ThisRUNGUID);
		_coordinator->SetKeyPair("NAME", GetOSNetwork().HostName);
		_coordinator->SetKeyPair("LASTUPDATETIME", "datetime('now')");
	}

	void SetNetGood(int8_t ID, bool bIsGood)
	{
		if (_bLastSent[ID] != bIsGood)
		{
			_bLastSent[ID] = bIsGood;
			JavascriptInterface::InvokeJS("SetNetGood", ID, bIsGood ? 1 : 0);
		}
	}

	void CloseRemoteAccess()
	{
		auto mapIPCMem = std::make_unique< IPCMappedMemory >("SPPAPPREMOTEHOST", 2 * 1024, false);
		if (mapIPCMem->IsValid())
		{
			auto memAcces = mapIPCMem->Lock();
			memAcces[1024] = 1;
			mapIPCMem->Release();
			std::this_thread::sleep_for(1s);
		}
	}

	void StartRemoteAccess()
	{
		CloseRemoteAccess();

		auto FullBinPath = SPP::GRootPath + "Binaries/" + REMOTE_ACCESS_APP;

		auto remoteAccessProcess = CreatePlatformProcess(FullBinPath.c_str(), "", true, false, false);

		if (remoteAccessProcess->IsValid())
		{
			for(int32_t Iter = 0; Iter < 20; Iter++)
			{
				auto mapIPCMem = std::make_unique< IPCMappedMemory >("SPPAPPREMOTEHOST", 2 * 1024, false);
				if (mapIPCMem->IsValid())
				{
					break;
				}
				std::this_thread::sleep_for(250ms);
			}
		}
	}

	void ValidateRemoteAccess()
	{
		if (!GAppConfig.remote.bEnabled)
		{
			SetNetGood(RemoteID, false);
			return;
		}

		auto mapIPCMem = std::make_unique< IPCMappedMemory >("SPPAPPREMOTEHOST", 2 * 1024, false);
		if (mapIPCMem->IsValid())
		{
			SetNetGood(RemoteID, true);
		}
		else
		{
			SetNetGood(RemoteID, false);
		}		
	}

	void UpdateRemoteDevices()
	{		
		Json::Value jsonData;
		for (auto& [key, value] : _remoteDevices)
		{
			auto dataRef = std::ref(value);
			Json::Value remoteJSONData;
			PODToJSON(dataRef, remoteJSONData);
			jsonData.append(remoteJSONData);
		}

		std::string oString;
		JsonToString(jsonData, oString);
		JavascriptInterface::InvokeJS("UpdateRemoteDevices", oString);
	}

	void Run()
	{
		_runThreadID = std::this_thread::get_id();
		
		CreateCoordinator();
		_juiceSocket = std::make_unique<UDPJuiceSocket>(GAppConfig.stun.addr.c_str(), GAppConfig.stun.port);

		//

		_broadReceiver = std::make_unique<UDPSocket>(GAppConfig.lan.port, UDPSocketOptions::Broadcast);
		

		//CHECK BROADCASTS
		std::vector<uint8_t> BufferRead;
		BufferRead.resize(std::numeric_limits<uint16_t>::max());

		_timer->AddTimer(1s, true, [&]()
		{
			ValidateRemoteAccess();
			UpdateRemoteDevices();
		});

		_timer->AddTimer(100ms, true, [&]()
		{
			IPv4_SocketAddress recvAddr;
			int32_t DataRecv = 0;
			while ((DataRecv = _broadReceiver->ReceiveFrom(recvAddr, BufferRead.data(), BufferRead.size())) > 0)
			{
				//SPP_LOG(LOG_APP, LOG_INFO, "UDP BROADCAST!!!");
				std::string HostString((char*)BufferRead.data(), (char*)BufferRead.data() + DataRecv);
				IPv4_SocketAddress hostPort(HostString.c_str());

				recvAddr.Port = hostPort.Port;
				auto realAddrOfConnection = recvAddr.ToString();

				_remoteDevices[realAddrOfConnection] = RemoteClient{
					std::chrono::steady_clock::now(),
					realAddrOfConnection,
					std::string(""),
					std::string("")
				};
			}
		});

		//ICE/STUN management
		_timer->AddTimer(100ms, true, [&]()
		{
			SetNetGood(JuiceeID, _juiceSocket && _juiceSocket->IsReady() && !_juiceSocket->HasProblem());

			if (_juiceSocket)
			{
				if (_juiceSocket->HasProblem())
				{
					_juiceSocket = std::make_unique<UDPJuiceSocket>(GAppConfig.stun.addr.c_str(), GAppConfig.stun.port);
					SPP_LOG(LOG_RD, LOG_INFO, "Resetting juice socket from problem (error on join usually)");
				}
				else if (_juiceSocket->IsReady())
				{
					if (_coordinator)
					{
						_coordinator->SetKeyPair("SDP", std::string(_juiceSocket->GetSDP_BASE64()));
					}
				}
			}			
			
			if (_coordinator)
			{
				SetNetGood(CoordID, _coordinator->IsConnected());
				_coordinator->Update();
			}
		});

		_timer->AddTimer(10ms, true, [&]()
		{
			_localThreadPool->RunOnce();
		});

		_timer->Run();
	}

};
std::unique_ptr< MainThreadApp > GMainApp;


void UpdateConfig(const std::string& InValue)
{
	Json::Value jsonData;
	if (StringToJson(InValue, jsonData))
	{
		APPConfig newConfig = GAppConfig;
		auto coordRef = std::ref(newConfig);
		JSONToPOD(coordRef, jsonData);

		JsonToFile("./remotedesktop.config.txt", jsonData);
		JsonToFile("./remoteaccess.config.txt", jsonData);

		GMainApp->UpdateConfig(newConfig);
	}	
}

void LoadConfigs()
{
	Json::Value jsonData;
	if (FileToJson("./remotedesktop.config.txt", jsonData))
	{
		auto coordRef = std::ref(GAppConfig);
		JSONToPOD(coordRef, jsonData);
	}
}

void PageLoaded()
{
	LoadConfigs();

	auto coordRef = std::ref(GAppConfig);
	Json::Value jsonData;
	PODToJSON(coordRef, jsonData);

	//JsonToFile("./remotedesktop.config.txt", jsonData);

	std::string oString;
	JsonToString(jsonData, oString);
	JavascriptInterface::InvokeJS("SetConfig", oString);

	// startup main app
	GMainApp = std::make_unique< MainThreadApp >();

}

SPP_AUTOREG_START
{
	using namespace rttr;
	registration::method("UpdateConfig", &UpdateConfig)
		.method("PageLoaded", &PageLoaded)
		.method("HelpClick", &HelpClick);

	rttr::registration::class_<RemoteClient>("RemoteClient")
		.property("Name", &RemoteClient::Name)(rttr::policy::prop::as_reference_wrapper)
		.property("AppName", &RemoteClient::AppName)(rttr::policy::prop::as_reference_wrapper)
		.property("AppCL", &RemoteClient::AppCL)(rttr::policy::prop::as_reference_wrapper)
		;
}
SPP_AUTOREG_END

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
	if (GMainApp)
	{
		GMainApp->Shutdown();
		GMainApp.reset();
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
	SPP::GRootPath = stdfs::absolute(stdfs::current_path() / "..\\").generic_string();
	SPP::GBinaryPath = SPP::GRootPath + "Binaries\\";
	SPP::GAssetPath = SPP::GRootPath + "Assets\\";

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


