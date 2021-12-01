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
#include "SPPPlatformCore.h"

#include "SPPFileSystem.h"

#include "SPPCrypto.h"

using namespace SPP;

LogEntry LOG_APP("APP");

struct RemoteClient
{
	std::chrono::steady_clock::time_point LastUpdate;
	std::string Name;
	std::string SDP;
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
	DataTransferConnection(std::shared_ptr< Interface_PeerConnection > InPeer) : NetworkConnection(InPeer, false)
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

	RSA_Cipher rsaServer;
	rsaServer.GenerateKeyPair(1024);

	auto rsaPublicKey = rsaServer.GetPublicKey();

	RSA_Cipher rsaClient(rsaPublicKey);

	AES_Cipher aescipher;
	aescipher.GenerateKey();
	auto symmetricKey = aescipher.GetKey();

	auto encryptedKey = rsaClient.EncryptString(symmetricKey);

	auto symmetricKeyCheck = rsaServer.DecryptString(encryptedKey);

	std::vector<uint8_t> DataCheck;
	DataCheck.resize(500 * 1024);
	for (int32_t Iter = 0; Iter < DataCheck.size(); Iter++)
	{
		DataCheck[Iter] = (uint8_t) (Iter % 256);
	}

	std::vector<uint8_t> encryptedData, decryptedData;
	aescipher.EncryptData(DataCheck.data(), DataCheck.size(), encryptedData);

	aescipher.DecryptData(encryptedData.data(), encryptedData.size(), decryptedData);

	SE_ASSERT(DataCheck == decryptedData);

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

	auto LastRequestJoins = std::chrono::steady_clock::now() - std::chrono::seconds(30);
	std::unique_ptr<UDP_SQL_Coordinator> coordinator = std::make_unique<UDP_SQL_Coordinator>(RemoteCoordAddres);

	coordinator->SetKeyPair("GUID", ThisRUNGUID);
	coordinator->SetKeyPair("NAME", GetOSNetwork().HostName);
	coordinator->SetKeyPair("LASTUPDATETIME", "datetime('now')");

	bool bInitiatedConnection = false;
	std::shared_ptr< DataTransferConnection > videoConnection;

	using namespace std::chrono_literals;

	std::vector<uint8_t> BufferRead;
	BufferRead.resize(1024);

	std::map<std::string, RemoteClient> Hosts;

	coordinator->SetSQLRequestCallback([&Hosts, &juiceSocket, localCoord = coordinator.get()](const std::string& InValue)
		{
			SPP_LOG(LOG_APP, LOG_INFO, "CALLBACK: %s", InValue.c_str());

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

				if (!ConnectToValue.isNull() && !SDPValue.isNull() && !GUIDValue.isNull())
				{
					localCoord->SetKeyPair("GUIDCONNECTTO", GUIDValue.asCString());
					juiceSocket->SetRemoteSDP_BASE64(SDPValue.asCString());
					return;
				}

				Json::Value NameValue = CurrentEle.get("NAME", Json::Value::nullSingleton());
				Json::Value GuidValue = CurrentEle.get("GUID", Json::Value::nullSingleton());

				std::string GUIDStr = GuidValue.asCString();
				{
					Hosts[GUIDStr] = RemoteClient{
						std::chrono::steady_clock::now(),
						std::string(NameValue.asCString())
					};
				}
			}
		});

	auto LastSentTime = std::chrono::steady_clock::now();
	auto LastRecvUpdateFromCoordinator = std::chrono::steady_clock::now() - std::chrono::seconds(30);

	while (true)
	{
		coordinator->Update();
		auto CurrentTime = std::chrono::steady_clock::now();

		//write status
		{
			bool IsConnectedToCoord = coordinator->IsConnected();

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
					SingleHost["GUID"] = key;
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
					if (key == GUIDStr)
					{
						if (juiceSocket->HasRemoteSDP() == false)
						{
							bInitiatedConnection = true;
							coordinator->SetKeyPair("GUIDCONNECTTO", key);
						}
					}
				}
			}

			memset(GUIDToJoin, 0, 6);
			ipcMem.WriteMemory(GUIDToJoin, 6, 1 * 1024 * 1024);
		}

		if (juiceSocket->IsReady())
		{
			coordinator->SetKeyPair("SDP", std::string(juiceSocket->GetSDP_BASE64()));

			if (!videoConnection &&
				std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - LastRequestJoins).count() > 1)
			{
				{
					auto SQLRequest = std::string_format("SELECT GUID, NAME FROM clients WHERE GUID != '%s'", ThisRUNGUID.c_str());
					coordinator->SQLRequest(SQLRequest.c_str());
				}
				{
					auto SQLRequest = std::string_format("SELECT * FROM clients WHERE GUIDCONNECTTO = '%s'", ThisRUNGUID.c_str());
					coordinator->SQLRequest(SQLRequest.c_str());
				}
				LastRequestJoins = CurrentTime;
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
				SPP_LOG(LOG_APP, LOG_INFO, "Connection dropped resetting sockets");
			}
			else if (videoConnection->IsConnected())
			{
				coordinator->SetKeyPair("GUIDCONNECTTO", "");
			}
		}
		else
		{
			if (juiceSocket->HasProblem())
			{
				juiceSocket = std::make_shared<UDPJuiceSocket>(StunURL.c_str(), StunPort);
				SPP_LOG(LOG_APP, LOG_INFO, "Resetting juice socket from problem (error on join usually)");
			}
			else if (juiceSocket->IsConnected())
			{
				videoConnection = std::make_shared< DataTransferConnection >(juiceSocket);
				videoConnection->CreateTranscoderStack(
					// allow reliability to UDP
					std::make_shared< ReliabilityTranscoder >(),
					// push on the splitter so we can ignore sizes
					std::make_shared< MessageSplitTranscoder >());
				videoConnection->Connect();
			}
		}

		// it has an active SDP response (gathering done)
		std::this_thread::sleep_for(1ms);
	}

	return 0;
}