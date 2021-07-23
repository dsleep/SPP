// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPNatTraversal.h"
#include "json/json.h"
#include "SPPLogging.h"

#include "SPPJsonUtils.h"

#include "SPPMemory.h"
#include "SPPNetworkConnection.h"
#include "SPPNetworkMessenger.h"
#include "SPPWin32Core.h"

#include <filesystem>

using namespace SPP;

LogEntry LOG_APP("APP");

struct RemoteClient
{
	std::chrono::steady_clock::time_point LastUpdate;
	std::string Name;
	std::string GUID;
};

/// <summary>
/// 
/// </summary>
class DataTransferConnection : public NetworkConnection
{
protected:
	std::chrono::high_resolution_clock::time_point LastImageCap;

	uint32_t ProcessID = 0;
	uint32_t SendSize = 20;
	std::vector<uint8_t> SendBuffer;
	std::vector<uint8_t> recvBuffer;

	SimplePolledRepeatingTimer< std::chrono::milliseconds > DataSendTimer;

public:
	DataTransferConnection(std::shared_ptr< Interface_PeerConnection > InPeer) : NetworkConnection(InPeer)
	{
		SendBuffer.resize(10 * 1024 * 1024);
		recvBuffer.resize(std::numeric_limits<uint16_t>::max());

		DataSendTimer.Initialize([this]()
			{
				CheckDataSend();
			}, 33);
	}

	virtual void MessageReceived(const void* Data, int32_t DataLength)
	{
	}

	void CheckDataSend()
	{
		if (GetBufferedAmount() < 2 * 1024 * 1024)
		{
			SendSize += 10;
			SendSize = std::min<size_t>(SendSize, SendBuffer.size());
			SendMessage(SendBuffer.data(), SendSize, EMessageMask::IS_RELIABLE);
		}
	}

	virtual void Tick() override
	{
		auto currentTime = SystemClock::now();

		while (true)
		{
			auto recvAmmount = _peerLink->Receive(recvBuffer.data(), recvBuffer.size());
			if (recvAmmount > 0)
			{
				ReceivedRawData(recvBuffer.data(), recvAmmount, 0);
			}
			else
			{
				break;
			}
		}
		
		NetworkConnection::Tick();				
		DataSendTimer.Poll();
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



IPv4_SocketAddress RemoteCoordAddres;
std::string StunURL;
uint16_t StunPort;

int main(int argc, char* argv[])
{
	IntializeCore(nullptr);
	{
		Json::Value JsonConfig;
		SE_ASSERT(FileToJson("config.txt", JsonConfig));

		Json::Value STUN_URL = JsonConfig.get("STUN_URL", Json::Value::nullSingleton());
		Json::Value STUN_PORT = JsonConfig.get("STUN_PORT", Json::Value::nullSingleton());
		Json::Value COORDINATOR_IP = JsonConfig.get("COORDINATOR_IP", Json::Value::nullSingleton());

		SE_ASSERT(!STUN_URL.isNull());
		SE_ASSERT(!STUN_PORT.isNull());
		SE_ASSERT(!COORDINATOR_IP.isNull());

		StunURL = STUN_URL.asCString();
		StunPort = STUN_PORT.asUInt();
		RemoteCoordAddres = IPv4_SocketAddress(COORDINATOR_IP.asCString());
	}

	std::string IPMemoryID;

	for (int i = 0; i < argc; ++i)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "CC(%d):%s", i, argv[i]);

		auto Arg = std::string(argv[i]);
		ParseCC(Arg, "-MEM=", IPMemoryID);
	}

	SPP_LOG(LOG_APP, LOG_INFO, "IPC MEMORY: %s", IPMemoryID.c_str());

	IPCMappedMemory ipcMem(IPMemoryID.c_str(), 2 * 1024 * 1024, false);

	SPP_LOG(LOG_APP, LOG_INFO, "IPC MEMORY VALID: %d", ipcMem.IsValid());

	// START OS NETWORKING
	GetOSNetwork();

	auto ThisRUNGUID = std::generate_hex(3);

	auto juiceSocket = std::make_shared<UDPJuiceSocket>(StunURL.c_str(), StunPort);
	auto coordSocket = std::make_shared<UDPSocket>();

	std::string ConnectToServer;
	std::shared_ptr< DataTransferConnection > videoConnection;

	using namespace std::chrono_literals;

	std::vector<uint8_t> BufferRead;
	BufferRead.resize(1024);

	std::map<std::string, RemoteClient> Hosts;

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

			if (Hosts.empty() == false)
			{
				Json::Value HostValues;
				for (auto& [key, value] : Hosts)
				{
					Json::Value SingleHost;
					SingleHost["NAME"] = value.Name;
					SingleHost["GUID"] = value.GUID;
					HostValues.append(SingleHost);
				}
				JsonMessage["HOSTS"] = HostValues;
			}

			if (videoConnection)
			{
				const auto& stats = videoConnection->GetStats();
				const auto& settings = videoConnection->GetSettings();
				
				JsonMessage["OUTGOINGLIMITKBS"] = settings.CCPerSecondRateLimitInBytes / 1024;
				JsonMessage["INCOMINGKBS"] = stats.LastKBsIncoming;
				JsonMessage["OUTGOINGKBS"] = stats.LastKBsOutgoing;
				JsonMessage["UPDATETIME"] = stats.LastCalcTimeFromAppStart;
				JsonMessage["OUTGOINGBUFFERSIZEKB"] = videoConnection->GetBufferedAmount() / 1024;			
				JsonMessage["OUTGOINGMESSAGECOUNT"] = videoConnection->GetBufferedMessageCount();
			}

			Json::StreamWriterBuilder wbuilder;
			std::string StrMessage = Json::writeString(wbuilder, JsonMessage);

			BinaryBlobSerializer outData;

			outData << (uint32_t)StrMessage.length();
			outData.Write(StrMessage.c_str(), StrMessage.length() + 1);
			ipcMem.WriteMemory(outData.GetData(), outData.Size());

			//GUID JOINING SYSTEM
			{
				// app wants to connect
				char GUIDToJoin[7];
				ipcMem.ReadMemory(GUIDToJoin, 6, 1 * 1024 * 1024);
				GUIDToJoin[6] = 0;

				if (GUIDToJoin[0])
				{
					std::string GUIDStr = GUIDToJoin;
					SPP_LOG(LOG_APP, LOG_INFO, "JOIN REQUEST!!!: %s", GUIDStr.c_str());

					for (auto& [key, value] : Hosts)
					{
						if (value.GUID == GUIDStr)
						{
							if (juiceSocket->HasRemoteSDP() == false)
							{
								juiceSocket->SetRemoteSDP_BASE64(key.c_str());
								ConnectToServer = value.GUID;
							}
						}
					}
				}

				memset(GUIDToJoin, 0, 6);
				ipcMem.WriteMemory(GUIDToJoin, 6, 1 * 1024 * 1024);
			}
		}

		// if we have a connection it handles it all
		if (videoConnection)
		{
			videoConnection->Tick();

			if (videoConnection->IsValid() == false)
			{
				videoConnection.reset();
				juiceSocket = std::make_shared<UDPJuiceSocket>(StunURL.c_str(), StunPort);
			}
		}
		else
		{
			if (juiceSocket->IsConnected())
			{
				videoConnection = std::make_shared< DataTransferConnection >(juiceSocket);
				videoConnection->CreateTranscoderStack(
					// allow reliability to UDP
					std::make_shared< ReliabilityTranscoder >(),
					// push on the splitter so we can ignore sizes
					std::make_shared< MessageSplitTranscoder >());
				if (!ConnectToServer.empty())
				{
					videoConnection->Connect();
				}
			}
		}

		// it has an active SDP response (gathering done)
		if (juiceSocket->IsReady())
		{
			if (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - LastSentTime).count() > 1)
			{
				Json::Value JsonMessage;

				JsonMessage["SENDALL"] = 1;
				JsonMessage["NAME"] = GetOSNetwork().HostName;
				JsonMessage["SDP"] = std::string(juiceSocket->GetSDP_BASE64());
				JsonMessage["GUID"] = ThisRUNGUID;
				if (!ConnectToServer.empty())
				{
					JsonMessage["CONNECT"] = ConnectToServer;
				}

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

				Json::Value NameValue = root.get("NAME", Json::Value::nullSingleton());
				Json::Value SDPValue = root.get("SDP", Json::Value::nullSingleton());
				Json::Value GuidValue = root.get("GUID", Json::Value::nullSingleton());

				if (SDPValue.isNull() == false &&
					GuidValue.isNull() == false &&
					NameValue.isNull() == false)
				{
					std::string SDPString = SDPValue.asCString();

					Hosts[SDPString] = RemoteClient{
						std::chrono::steady_clock::now(),
						std::string(NameValue.asCString()),
						std::string(GuidValue.asCString()) };
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