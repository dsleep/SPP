// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include <windows.h>	

#ifdef SendMessage
#undef SendMessage
#endif

#include "SPPCore.h"
#include "SPPNatTraversal.h"
#include "json/json.h"
#include "SPPLogging.h"

#include "SPPMemory.h"

#include "SPPCapture.h"
#include "SPPVideo.h"

#include "SPPNetworkConnection.h"
#include "SPPNetworkMessenger.h"
#include "SPPPlatformCore.h"
#include "SPPJsonUtils.h"
#include "SPPReflection.h"
#include "SPPFileSystem.h"

#include "SPPHandledTimers.h"
#include "SPPApplication.h"

SPP_OVERLOAD_ALLOCATORS

using namespace SPP;

LogEntry LOG_APP("APP");

#define PREVENT_INPUT 1

HINSTANCE GhInstance = nullptr;

#include "./SPPRemoteApplicationwxWidgets/SPP_RD_AppConfig.inl"

APPConfig GAppConfig;

struct handle_data {
	uint32_t process_id;
	HWND window_handle;
};

BOOL is_main_window(HWND handle)
{
	return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
	handle_data& data = *(handle_data*)lParam;
	uint32_t process_id = 0;
	GetWindowThreadProcessId(handle, (LPDWORD)&process_id);
	if (data.process_id != process_id || !is_main_window(handle))
		return TRUE;
	data.window_handle = handle;
	return FALSE;
}

HWND find_main_window(uint32_t process_id)
{
	handle_data data;
	data.process_id = process_id;
	data.window_handle = 0;
	EnumWindows(enum_windows_callback, (LPARAM)&data);
	return data.window_handle;
}

const uint8_t KeyBoardMessage = 0x02;
const uint8_t MouseMessage = 0x03;
const uint8_t BTMessage = 0x04;

const uint8_t MS_ButtonOrMove = 0x01;
const uint8_t MS_Window = 0x02;


struct IPCMotionState
{
	int32_t buttonState[2];
	float motionXY[2];
	float orientationQuaternion[4];
};


/// <summary>
/// 
/// </summary>
class VideoConnection : public NetworkConnection
{
protected:
	std::unique_ptr< VideoEncodingInterface> VideoEncoder;

	std::unique_ptr< IPCMappedMemory> _mappedSofaMem;
	std::unique_ptr< SimpleIPCMessageQueue<IPCMotionState> > _msgQueue;

	std::chrono::high_resolution_clock::time_point LastImageCap;

	uint32_t ProcessID = 0;
	std::vector<uint8_t> ImageData;
	std::vector<uint8_t> recvBuffer;

	HWND CurrentLinkedApp = nullptr;

	std::string AppPath;
	std::string AppCommandline;

	uint32_t _lastBuzzCnt = 0;
	uint32_t _currentBuzzCnt = 0;

public:
	VideoConnection(std::shared_ptr< Interface_PeerConnection > InPeer, const std::string &InAppPath, const std::string &AppCommandline) : NetworkConnection(InPeer, true)
	{
		recvBuffer.resize(std::numeric_limits<uint16_t>::max());
		
		if (!InAppPath.empty())
		{
			auto MemShareID = std::generate_hex(3);
			std::string WithMemShare = AppCommandline + std::string_format(" --MEMSHARE=%s", MemShareID.c_str());

			//IPC TO SHARE WITH SOFA
			_mappedSofaMem = std::make_unique<IPCMappedMemory>(MemShareID.c_str(), sizeof(IPCMotionState) * 200, true);
			_msgQueue = std::make_unique< SimpleIPCMessageQueue<IPCMotionState> >(*_mappedSofaMem, sizeof(_currentBuzzCnt));
			ProcessID = CreateChildProcess(InAppPath.c_str(), WithMemShare.c_str());
		}
	}

	virtual ~VideoConnection()
	{
		if (ProcessID)
		{
			CloseChild(ProcessID);
		}

		ProcessID = 0;
		if (VideoEncoder)
		{
			VideoEncoder->Finalize();
			VideoEncoder.reset();
		}
	}

	virtual void MessageReceived(const void* Data, int32_t DataLength)
	{
		//SPP_LOG(LOG_APP, LOG_INFO, "ApplicationHost::MessageReceived %d", DataLength);

#if PREVENT_INPUT
		return;
#endif
	
		{
			MemoryView DataView(Data, DataLength);
			uint8_t msgType;

			DataView >> msgType;

			if (msgType == KeyBoardMessage)
			{
				uint8_t bDown = 0;
				int32_t keyCode = 0;

				DataView >> bDown;
				DataView >> keyCode;
				
#if _WIN32
				if (CurrentLinkedApp)
				{

					UINT uMsg = bDown ? WM_KEYDOWN : WM_KEYUP;

					//NK_LSHIFT 306
					//NK_RSHIFT 306
					//NK_LCTRL 308
					//NK_RCTRL 308

					switch (keyCode)
					{
					case 306:
						keyCode = VK_SHIFT;
						break;
					case 308:
						keyCode = VK_CONTROL;
						break;
					case 340: //F1
					case 341:
					case 342:
					case 343:
					case 344:
					case 345:
					case 346:
					case 347:
					case 348:
					case 349:
					case 350:
					case 351: //F12				
						keyCode = (keyCode - 340) + VK_F1;
						break;
					}

					WPARAM wParam = keyCode;
					LPARAM lParam = 0;

					PostMessage(CurrentLinkedApp, uMsg, wParam, lParam);
				}
				else
				{
					INPUT inputs = {};
					ZeroMemory(&inputs, sizeof(inputs));

					inputs.type = INPUT_KEYBOARD;
					if (!bDown)
					{
						inputs.ki.dwFlags = KEYEVENTF_KEYUP;
					}

					switch (keyCode)
					{
					case 306:
						keyCode = VK_SHIFT;
						break;
					case 308:
						keyCode = VK_CONTROL;
						break;
					case 340: //F1
					case 341:
					case 342:
					case 343:
					case 344:
					case 345:
					case 346:
					case 347:
					case 348:
					case 349:
					case 350:
					case 351: //F12				
						keyCode = (keyCode - 340) + VK_F1;
						break;
					}

					inputs.ki.wVk = keyCode;

					SendInput(1,&inputs,sizeof(INPUT));
				}
#endif

				//SPP_LOG(LOG_APP, LOG_INFO, "KEYEVENT Down: %d, KC: %d", bDown, keyCode);
			}
			else if (msgType == MouseMessage)
			{				
				uint16_t posX = 0;
				uint16_t posy = 0;
				uint8_t MouseMSGType = 0;
				DataView >> MouseMSGType;

				if (MouseMSGType == MS_Window)
				{
					uint8_t EnterState = 0;
					DataView >> EnterState;
					DataView >> posX;
					DataView >> posy;

#if _WIN32
					if (CurrentLinkedApp)
					{
						if (EnterState)
						{
							LPARAM lParam = posX | ((LPARAM)posy << 16);
							PostMessage(CurrentLinkedApp, WM_NCMOUSEMOVE, 0, lParam);
						}
						else
						{
							PostMessage(CurrentLinkedApp, WM_NCMOUSELEAVE, 0, 0);
						}
					}
#endif
				}
				else if (MouseMSGType == MS_ButtonOrMove)
				{
					uint8_t ActualButton = 0;
					uint8_t bDown = 0;
					uint8_t DownState = 0;
					int8_t MouseWheel = 0;

					DataView >> ActualButton;
					DataView >> bDown;
					DataView >> DownState;
					DataView >> posX;
					DataView >> posy;
					DataView >> MouseWheel;

#if _WIN32
					if (CurrentLinkedApp)
					{
						WPARAM wParam = 0;

						if (DownState & 0x01)
						{
							wParam |= MK_LBUTTON;
						}
						if (DownState & 0x02)
						{
							wParam |= MK_MBUTTON;
						}
						if (DownState & 0x04)
						{
							wParam |= MK_RBUTTON;
						}

						UINT uMsg = 0;
						LPARAM lParam = posX | ((LPARAM)posy << 16);

						if (MouseWheel != 0)
						{
							uMsg = WM_MOUSEWHEEL;
							wParam |= ((LPARAM)MouseWheel << 16);
						}
						else
						{
							switch (ActualButton)
							{
							case 1:
								uMsg = bDown ? WM_LBUTTONDOWN : WM_LBUTTONUP;
								break;
							case 2:
								uMsg = bDown ? WM_MBUTTONDOWN : WM_MBUTTONUP;
								break;
							case 3:
								uMsg = bDown ? WM_RBUTTONDOWN : WM_RBUTTONUP;
								break;
							default:
								uMsg = WM_MOUSEMOVE;
								break;
							}
						}

						PostMessage(CurrentLinkedApp, uMsg, wParam, lParam);
					}
					else
					{
						INPUT inputs = {};
						ZeroMemory(&inputs, sizeof(inputs));

						double fScreenWidth = ::GetSystemMetrics(SM_CXSCREEN) - 1;
						double fScreenHeight = ::GetSystemMetrics(SM_CYSCREEN) - 1;
						double fx = posX * (65535.0 / fScreenWidth);
						double fy = posy * (65535.0 / fScreenHeight);

						inputs.type = INPUT_MOUSE;
						inputs.mi.mouseData = 0;
						inputs.mi.dx = fx; 
						inputs.mi.dy = fy;
						inputs.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

						SendInput(1, &inputs, sizeof(INPUT));

						
						if (MouseWheel != 0)
						{
							ZeroMemory(&inputs, sizeof(inputs));
							inputs.type = INPUT_MOUSE;
							inputs.mi.dwFlags = MOUSEEVENTF_WHEEL;
							inputs.mi.mouseData = (DWORD)MouseWheel; //A positive value indicates that the wheel was rotated forward, away from the user; a negative value indicates that the wheel was rotated backward, toward the user. One wheel click is defined as WHEEL_DELTA, which is 120.
							SendInput(1, &inputs, sizeof(INPUT));
						}
						else
						{
							DWORD mouseButtonFlags = 0;

							switch (ActualButton)
							{
							case 1:
								mouseButtonFlags = bDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
								break;
							case 2:
								mouseButtonFlags = bDown ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
								break;
							case 3:
								mouseButtonFlags = bDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
								break;
							default:
								break;
							}

							if (mouseButtonFlags)
							{
								ZeroMemory(&inputs, sizeof(inputs));
								inputs.type = INPUT_MOUSE;
								inputs.mi.dwFlags = mouseButtonFlags;
								SendInput(1, &inputs, sizeof(INPUT));
							}
						}

						

					}
#endif
				}
			}
			//BT message
			else if (msgType == BTMessage)
			{
				std::string JsonMessage;
				DataView >> JsonMessage;

				SPP_LOG(LOG_APP, LOG_INFO, "ApplicationHost::MessageReceived JSON %s", JsonMessage.c_str());

				Json::Value jsonMessageParsed;
				if (StringToJson(JsonMessage, jsonMessageParsed))
				{
					Json::Value dataValue = jsonMessageParsed.get("data", Json::Value::nullSingleton());
					if (!dataValue.isNull())
					{
						std::string DataSet = dataValue.asCString();
						auto SplitData = std::str_split(DataSet, ',');

						if (SplitData.size() >= 8)
						{
							IPCMotionState newMessage;
							newMessage.buttonState[0] = std::atoi(SplitData[0].c_str());
							newMessage.buttonState[1] = std::atoi(SplitData[1].c_str());

							newMessage.motionXY[0] = std::atof(SplitData[2].c_str());
							newMessage.motionXY[1] = std::atof(SplitData[3].c_str());

							newMessage.orientationQuaternion[0] = std::atof(SplitData[4].c_str());
							newMessage.orientationQuaternion[1] = std::atof(SplitData[5].c_str());
							newMessage.orientationQuaternion[2] = std::atof(SplitData[6].c_str());
							newMessage.orientationQuaternion[3] = std::atof(SplitData[7].c_str());

							_msgQueue->PushMessage(newMessage);
						}

						/* SOFA TIPS EXAMPLE CODE:

						struct IPCMotionState
						{
							int32_t buttonState[2];
							float motionXY[2];
							float orientationQuaternion[4];
						};

						//parsed from -MEMSHARE= commandline argument
						std::string MemShareID;
						std::unique_ptr< IPCMappedMemory> _mappedSofaMem;
						std::unique_ptr< SimpleIPCMessageQueue<IPCMotionState> > _msgQueue;
						_mappedSofaMem = std::make_unique<IPCMappedMemory>(MemShareID.c_str(), sizeof(IPCMotionState) * 200, false);
						_msgQueue = std::make_unique< SimpleIPCMessageQueue<IPCMotionState> >(*_mappedSofaMem, sizeof(uint32_t));

						// get all BT messages, and it will auto clear them
						auto Messages = _msgQueue->GetMessages();
						for (auto& curMessage : Messages)
						{
							//curMessage.buttonState[0]
							//curMessage.motionXY[0]
							//curMessage.orientationQuaternion[0]
						}

						// send buzz back
						static uint32_t buzzCounter = 1;
						_mappedSofaMem->WriteMemory(&buzzCounter, sizeof(buzzCounter));
						buzzCounter++;
						*/
					}
				}
			}
		}
	}

	void CheckFeedbackFromSofa()
	{
		if (_mappedSofaMem)
		{
			_mappedSofaMem->ReadMemory(&_currentBuzzCnt, sizeof(_currentBuzzCnt));
			if (_lastBuzzCnt != _currentBuzzCnt)
			{
				// buzz is 2
				BinaryBlobSerializer thisFrame;
				thisFrame << (uint8_t)2;
				SendMessage(thisFrame.GetData(), thisFrame.Size(), EMessageMask::IS_RELIABLE);
				_lastBuzzCnt = _currentBuzzCnt;
			}
		}
	}

	virtual void Tick() override
	{
		if (!_peerLink->IsWrappedPeer())
		{
			int32_t recvAmmount = 0;
			while ((recvAmmount = _peerLink->Receive(recvBuffer.data(), recvBuffer.size())) > 0)
			{
				ReceivedRawData(recvBuffer.data(), recvAmmount, 0);
			}
		}

		if (ProcessID)
		{
			if (IsChildRunning(ProcessID) == false)
			{
				CloseDown("Process Closed!");
			}
		}
				
		NetworkConnection::Tick();

		if (_networkState == EConnectionState::CONNECTED)
		{
			CheckFeedbackFromSofa();
			CheckSendImage();
		}
	}

	void ValidateEncoder(int32_t Width, int32_t Height)
	{
		if (VideoEncoder)
		{
			const auto& vidSettings = VideoEncoder->GetVideoSettings();
			if (vidSettings.width != Width || vidSettings.height != Height)
			{
				VideoEncoder->Finalize();
				VideoEncoder.reset();
			}
		}

		if (!VideoEncoder)
		{
			SPP_LOG(LOG_APP, LOG_INFO, "ApplicationHost::CreateVideoEncoder %d x %d", Width, Height);

			VideoEncoder = CreateVideoEncoder([CreatedWidth = Width,CreatedHeight= Height,this](const void* InData, int32_t InDataSize)
				{					
					BinaryBlobSerializer thisFrame;
					thisFrame << (uint8_t)1;
					thisFrame << (uint16_t)CreatedWidth;
					thisFrame << (uint16_t)CreatedHeight;
					thisFrame.Write(InData, InDataSize);
					SendMessage(thisFrame.GetData(), thisFrame.Size(), EMessageMask::IS_RELIABLE);		

				}, VideoSettings{ Width, Height, 4, 3, 32 }, {});
		}


		CurrentLinkedApp = find_main_window(ProcessID);
	}

	void CheckSendImage()
	{
		if(GetBufferedAmount() > 1 * 1024 * 1024)
		{
			//SPP_LOG(LOG_APP, LOG_INFO, "ApplicationHost::Drop Frame");
			return;
		}

		auto CurrentTime = std::chrono::high_resolution_clock::now();
		auto milliTime = std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - LastImageCap).count();
		if (milliTime > 42)
		{
			int32_t Width = 0;
			int32_t Height = 0;
			uint8_t BytesPP = 0;
			if (CaptureApplicationWindow(ProcessID, Width, Height, ImageData, BytesPP))
			{
				ValidateEncoder(Width, Height);
				VideoEncoder->Encode(ImageData.data(), ImageData.size());
				LastImageCap = CurrentTime;
			}
		}
	}
};


/// <summary>
/// 
/// </summary>
/// <param name="ThisRUNGUID"></param>
/// <param name="SimpleAppName"></param>
/// <param name="AppCommandline"></param>
/// <param name="ClientRequestCommandline"></param>
/// <param name="AppPath"></param>
/// <param name="ipcMem"></param>
void _mainThread(const std::string& ThisRUNGUID,
	const std::string& SimpleAppName,
	const std::string& AppCommandline,
	std::string ClientRequestCommandline,
	const std::string& AppPath,
	IPCMappedMemory& ipcMem,
	bool bLANOnly)
{
	std::unique_ptr<ApplicationWindow> app = CreateApplication();

	app->Initialize(128, 128, GhInstance);
	app->CreateNotificationIcon();

	std::shared_ptr<UDPSocket> broadcastSocket = std::make_shared<UDPSocket>(0, UDPSocketOptions::Broadcast);
	std::shared_ptr<UDPSocket> serverSocket = std::make_shared<UDPSocket>();
	std::shared_ptr< UDPSendWrapped > videoSocket;
	std::shared_ptr< VideoConnection > videoConnection;

	std::shared_ptr<UDPJuiceSocket> juiceSocket;
	std::unique_ptr<UDP_SQL_Coordinator> coordinator;

	if (!bLANOnly)
	{
		juiceSocket = std::make_shared<UDPJuiceSocket>(GAppConfig.stun.addr.c_str(), GAppConfig.stun.port);
		coordinator = std::make_unique<UDP_SQL_Coordinator>(GAppConfig.coord.addr.c_str());

		coordinator->SetPassword(GAppConfig.coord.pwd);
		coordinator->SetKeyPair("GUID", ThisRUNGUID);
		coordinator->SetKeyPair("APPNAME", SimpleAppName);
		coordinator->SetKeyPair("NAME", GetOSNetwork().HostName);
		coordinator->SetKeyPair("LASTUPDATETIME", "datetime('now')");
		coordinator->SetKeyPair("APPCL", AppCommandline);

		coordinator->SetSQLRequestCallback([&juiceSocket, localCoord = coordinator.get(), &ClientRequestCommandline](const std::string& InValue)
		{
			SPP_LOG(LOG_APP, LOG_INFO, "CALLBACK: %s", InValue.c_str());

			if (juiceSocket->HasRemoteSDP() == false)
			{
				Json::Value root;
				Json::CharReaderBuilder Builder;
				Json::CharReader* reader = Builder.newCharReader();
				std::string Errors;

				bool parsingSuccessful = reader->parse((char*)InValue.data(), (char*)(InValue.data() + InValue.length()), &root, &Errors);
				delete reader;
				if (!parsingSuccessful)
				{
					return;
				}

				for (int32_t Iter = 0; Iter < root.size(); Iter++)
				{
					auto CurrentEle = root[Iter];

					Json::Value GUIDValue = CurrentEle.get("GUID", Json::Value::nullSingleton());
					Json::Value ConnectToValue = CurrentEle.get("GUIDCONNECTTO", Json::Value::nullSingleton());
					Json::Value SDPValue = CurrentEle.get("SDP", Json::Value::nullSingleton());
					Json::Value APPCLValue = CurrentEle.get("APPCL", Json::Value(""));

					if (!ConnectToValue.isNull() && !SDPValue.isNull() && !GUIDValue.isNull())
					{
						localCoord->SetKeyPair("GUIDCONNECTTO", GUIDValue.asCString());
						ClientRequestCommandline = APPCLValue.asCString();
						juiceSocket->SetRemoteSDP_BASE64(SDPValue.asCString());
						return;
					}
				}
			}
		});
	}

	using namespace std::chrono_literals;

	std::vector<uint8_t> BufferRead;
	BufferRead.resize(std::numeric_limits<uint16_t>::max());

	TimerController mainController(16ms);

	//IPC UPDATES
	mainController.AddTimer(500ms, true, [&]()
		{			
			auto memAcces = ipcMem.Lock();
			if (memAcces[1024])
			{
				mainController.Stop();
			}
			ipcMem.Release();			
		});

	// COORDINATOR UPDATES
	auto LastSQLQuery = std::chrono::steady_clock::now() - std::chrono::seconds(30);
	mainController.AddTimer(50ms, true, [&]()
		{
			coordinator->Update();
			auto CurrentTime = std::chrono::steady_clock::now();
			if (juiceSocket && juiceSocket->IsReady())
			{
				coordinator->SetKeyPair("SDP", std::string(juiceSocket->GetSDP_BASE64()));

				if (!videoConnection &&
					std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - LastSQLQuery).count() > 1)
				{
					auto SQLRequest = std::string_format("SELECT * FROM clients WHERE GUIDCONNECTTO = '%s'", ThisRUNGUID.c_str());
					coordinator->SQLRequest(SQLRequest.c_str());
					LastSQLQuery = CurrentTime;
				}
			}
		});

	//UDP BEACON
	IPv4_SocketAddress broadcastAddr("255.255.255.255", GAppConfig.lan.port);
	IPv4_SocketAddress localAddr = serverSocket->GetLocalAddress();
	std::string socketData = std::string_format("%6.6s%6u%24.24s", ThisRUNGUID.c_str(), localAddr.Port, GetOSNetwork().HostName.c_str());

	mainController.AddTimer(1s, true, [&]()
		{
			if (!videoConnection)
			{	
				broadcastSocket->SendTo(broadcastAddr, socketData.c_str(), socketData.length());
			}
		});

	//NET UPDATES
	mainController.AddTimer(16.6666ms, true, [&]()
		{
			IPv4_SocketAddress recvAddr;
			int32_t DataRecv = 0;
			while ((DataRecv = serverSocket->ReceiveFrom(recvAddr, BufferRead.data(), BufferRead.size())) > 0)
			{
				if (!videoConnection)
				{					
					videoSocket = std::make_shared<UDPSendWrapped>(serverSocket, recvAddr);
					videoConnection = std::make_shared< VideoConnection >(videoSocket, "", "");
					videoConnection->CreateTranscoderStack(
						// allow reliability to UDP
						std::make_shared< ReliabilityTranscoder >(),
						// push on the splitter so we can ignore sizes
						std::make_shared< MessageSplitTranscoder >());
				}

				SE_ASSERT(videoSocket && videoConnection);

				if (videoSocket->GetRemoteAddress() == recvAddr)
				{
					videoConnection->ReceivedRawData(BufferRead.data(), DataRecv, 0);
				}
			}

			// if we have a connection it handles it all
			if (videoConnection)
			{
				videoConnection->Tick();

				if (videoConnection->IsValid() == false)
				{
					videoConnection.reset();
					SPP_LOG(LOG_APP, LOG_INFO, "Connection dropped...");
				}
			}
			else if (juiceSocket)
			{
				if (juiceSocket->HasProblem())
				{
					juiceSocket = std::make_shared<UDPJuiceSocket>(GAppConfig.stun.addr.c_str(), GAppConfig.stun.port);
					SPP_LOG(LOG_APP, LOG_INFO, "Resetting juice socket from problem (error on join usually)");
				}
				else if (juiceSocket->IsConnected())
				{
					videoConnection = std::make_shared< VideoConnection >(juiceSocket, "", "");
					videoConnection->CreateTranscoderStack(
						// allow reliability to UDP
						std::make_shared< ReliabilityTranscoder >(),
						// push on the splitter so we can ignore sizes
						std::make_shared< MessageSplitTranscoder >());
				}
			}			

			if (app->RunOnce() < 0)
			{
				mainController.Stop();
			}
		});

	mainController.Run();
}



int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	GhInstance = hInstance;
	IntializeCore(nullptr);

	std::vector<MonitorInfo> Infos;
	GetMonitorInfo(Infos);

	{
		//Json::Value JsonConfig;
		//SE_ASSERT(FileToJson("remoteaccess.config.txt", JsonConfig));

		Json::Value jsonData;
		SE_ASSERT(FileToJson("./remoteaccess.config.txt", jsonData));
		
		auto coordRef = std::ref(GAppConfig);
		JSONToPOD(coordRef, jsonData);		
	}

	auto ThisRUNGUID = std::generate_hex(3);
	AddDLLSearchPath("../3rdParty/libav_CUDA/bin");

	std::string ClientRequestCommandline;

	//auto CCMap = std::BuildCCMap(argc, argv);
	//auto IPMemoryID = MapFindOrNull(CCMap, "MEM");
	std::string AppPath = "";// MapFindOrDefault(CCMap, "APP");
	std::string AppCommandline = "";// MapFindOrDefault(CCMap, "CMDLINE");
	std::string lanonlyCC = "";// MapFindOrDefault(CCMap, "lanonly");

	//SE_ASSERT(IPMemoryID);

	//SPP_LOG(LOG_APP, LOG_INFO, "IPC MEMORY: %s", IPMemoryID->c_str());
	SPP_LOG(LOG_APP, LOG_INFO, "EXE PATH: %s", AppPath.c_str());
	SPP_LOG(LOG_APP, LOG_INFO, "APP COMMAND LINE: %s", AppCommandline.c_str());

	IPCMappedMemory ipcMem("SPPAPPREMOTEHOST", 2 * 1024, true);

	SPP_LOG(LOG_APP, LOG_INFO, "IPC MEMORY VALID: %d", ipcMem.IsValid());
	SPP_LOG(LOG_APP, LOG_INFO, "RUN GUID: %s", ThisRUNGUID.c_str());

	// START OS NETWORKING
	GetOSNetwork();

	std::string SimpleAppName = AppPath.empty() ? "Desktop" : stdfs::path(AppPath).stem().generic_string();

	_mainThread(ThisRUNGUID,
		SimpleAppName,
		AppCommandline,
		ClientRequestCommandline,
		AppPath,
		ipcMem,
		lanonlyCC.length() ? true : false);
	
	return 0;
}