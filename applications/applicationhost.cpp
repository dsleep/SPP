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
#include "SPPWin32Core.h"

#include <filesystem>

using namespace SPP;

LogEntry LOG_APP("APP");

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

/// <summary>
/// 
/// </summary>
class VideoConnection : public NetworkConnection
{
protected:
	std::unique_ptr< VideoEncodingInterface> VideoEncoder;
	std::chrono::high_resolution_clock::time_point LastImageCap;

	uint32_t ProcessID = 0;
	std::vector<uint8_t> ImageData;
	std::vector<uint8_t> recvBuffer;

	HWND CurrentLinkedApp = nullptr;

	std::string AppPath;
	std::string AppCommandline;

public:
	VideoConnection(std::shared_ptr< Interface_PeerConnection > InPeer, const std::string &InAppPath, const std::string &AppCommandline) : NetworkConnection(InPeer)
	{
		recvBuffer.resize(std::numeric_limits<uint16_t>::max());

		if (!InAppPath.empty())
		{
			ProcessID = CreateChildProcess(InAppPath.c_str(), AppCommandline.c_str());
		}
	}

	virtual void MessageReceived(const void* Data, int32_t DataLength)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "ApplicationHost::MessageReceived %d", DataLength);

		MemoryView DataView(Data, DataLength);
		UINT uMsg;
		WPARAM wParam;
		LPARAM lParam;
		DataView >> uMsg;
		DataView >> wParam;
		DataView >> lParam;

		if (CurrentLinkedApp)
		{
			PostMessage(CurrentLinkedApp, uMsg, wParam, lParam);
		}
	}

	virtual void Tick() override
	{
		auto recvAmmount = _peerLink->Receive(recvBuffer.data(), recvBuffer.size());
		if (recvAmmount > 0)
		{
			ReceivedRawData(recvBuffer.data(), recvAmmount, 0);
		}

		NetworkConnection::Tick();

		CheckSendImage();
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
		if (_outGoingStream.Size() > 3 * 1024 * 1024)
		{
			SPP_LOG(LOG_APP, LOG_INFO, "ApplicationHost::Drop Frame");
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

bool ParseCC(const std::string& InCmdLn, const std::string& InValue, std::string& OutValue)
{
	if (StartsWith(InCmdLn, InValue))
	{
		OutValue = std::string(InCmdLn.begin() + InValue.size(), InCmdLn.end());
		return true;
	}
	return false;
}

IPv4_SocketAddress RemoteCoordAddres("70.185.114.136", 12021);

int main(int argc, char* argv[])
{
	IntializeCore(nullptr);

	AddDLLSearchPath("../3rdParty/libav_CUDA/bin");

	std::string IPMemoryID;
	std::string AppPath;
	std::string AppCommandline;

	for (int i = 0; i < argc; ++i)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "CC(%d):%s", i, argv[i]);

		auto Arg = std::string(argv[i]);
		ParseCC(Arg, "-MEM=", IPMemoryID);
		ParseCC(Arg, "-APP=", AppPath);
		ParseCC(Arg, "-CMDLINE=", AppCommandline);
	}

	SPP_LOG(LOG_APP, LOG_INFO, "IPC MEMORY: %s", IPMemoryID.c_str());
	SPP_LOG(LOG_APP, LOG_INFO, "EXE PATH: %s", AppPath.c_str());
	SPP_LOG(LOG_APP, LOG_INFO, "APP COMMAND LINE: %s", AppCommandline.c_str());

	IPCMappedMemory ipcMem(IPMemoryID.c_str(), 1 * 1024 * 1024, false);

	SPP_LOG(LOG_APP, LOG_INFO, "IPC MEMORY VALID: %d", ipcMem.IsValid());

	// START OS NETWORKING
	GetOSNetwork();

	std::string SimpleAppName = AppPath.empty() ? "Desktop" : std::filesystem::path(AppPath).stem().generic_string();

	auto ThisRUNGUID = std::generate_hex(3);

	auto juiceSocket = std::make_shared<UDPJuiceSocket>();
	auto coordSocket = std::make_shared<UDPSocket>();
	std::shared_ptr< VideoConnection > videoConnection;

	using namespace std::chrono_literals;

	std::vector<uint8_t> BufferRead;
	BufferRead.resize(1024);

	auto LastSentTime = std::chrono::steady_clock::now();
	auto LastRecvUpdateFromCoordinator = std::chrono::steady_clock::now() - std::chrono::seconds(30);

	while (true)
	{
		auto CurrentTime = std::chrono::steady_clock::now();

		//write status
		{
			bool IsConnectedToCoord = (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - LastRecvUpdateFromCoordinator).count() < 4);

			Json::Value JsonMessage;
			JsonMessage["COORD"] = IsConnectedToCoord;
			JsonMessage["RESOLVEDSDP"] = (juiceSocket && juiceSocket->IsReady());
			JsonMessage["CONNECTED"] = (videoConnection && videoConnection->IsValid());

			Json::StreamWriterBuilder wbuilder;
			std::string StrMessage = Json::writeString(wbuilder, JsonMessage);

			BinaryBlobSerializer outData;

			outData << (uint32_t)StrMessage.length();
			outData.Write(StrMessage.c_str(), StrMessage.length() + 1);
			ipcMem.WriteMemory(outData.GetData(), outData.Size());
		}

		// if we have a connection it handles it all
		if (videoConnection)
		{
			videoConnection->Tick();

			if (videoConnection->IsValid() == false)
			{
				videoConnection.reset();
				juiceSocket = std::make_shared<UDPJuiceSocket>();
			}
		}
		else
		{
			if (juiceSocket->IsConnected())
			{
				videoConnection = std::make_shared< VideoConnection >(juiceSocket, AppPath, AppCommandline);
				videoConnection->CreateTranscoderStack(
					// allow reliability to UDP
					std::make_shared< ReliabilityTranscoder >(),
					// push on the splitter so we can ignore sizes
					std::make_shared< MessageSplitTranscoder >());
			}
		}

		// it has an active SDP response (gathering done)
		if (juiceSocket->IsReady())
		{
			if (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - LastSentTime).count() > 1)
			{
				Json::Value JsonMessage;

				JsonMessage["APPNAME"] = SimpleAppName;
				JsonMessage["NAME"] = GetOSNetwork().HostName;
				JsonMessage["SDP"] = std::string(juiceSocket->GetSDP_BASE64());
				JsonMessage["GUID"] = ThisRUNGUID;
				JsonMessage["CONNECTED"] = (videoConnection && videoConnection->IsValid());

				Json::StreamWriterBuilder wbuilder;
				std::string StrMessage = Json::writeString(wbuilder, JsonMessage);
				coordSocket->SendTo(RemoteCoordAddres, StrMessage.c_str(), StrMessage.size());
				LastSentTime = std::chrono::steady_clock::now();
			}

			// send we sent we should get a reponse from coord
			IPv4_SocketAddress currentAddress;
			auto packetSize = coordSocket->ReceiveFrom(currentAddress, BufferRead.data(), BufferRead.size() - 1);

			if (packetSize > 0)
			{
				Json::Value root;
				Json::CharReaderBuilder Builder;
				Json::CharReader* reader = Builder.newCharReader();
				std::string Errors;

				bool parsingSuccessful = reader->parse((char*)BufferRead.data(), (char*)(BufferRead.data() + packetSize), &root, &Errors);
				delete reader;
				if (!parsingSuccessful)
				{
					break;
				}

				Json::Value ServerTime = root.get("SERVERTIME", Json::Value::nullSingleton());
				if (ServerTime.isNull() == false)
				{
					LastRecvUpdateFromCoordinator = std::chrono::steady_clock::now();
				}

				if (juiceSocket->HasRemoteSDP() == false)
				{
					Json::Value ClientConnection = root.get("CONNECTSDP", Json::Value::nullSingleton());

					if (ClientConnection.isNull() == false)
					{
						std::string SDPString = ClientConnection.asCString();
						SPP_LOG(LOG_APP, LOG_INFO, "Recv Remote SDP: starting connetion to %s", SDPString.c_str());
						juiceSocket->SetRemoteSDP_BASE64(SDPString.c_str());
					}
				}
			}
		}

		std::this_thread::sleep_for(1ms);
	}

	return 0;
}