// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "json/json.h"
#include "SPPLogging.h"
#include <set>

#if USE_LIBDATACHANNEL
	#include "SPPWebSockets.h"
#endif

#include "SPPMemory.h"
#include "SPPPlatformCore.h"

#include "SPPNetworkConnection.h"
#include "SPPNetworkMessenger.h"

using namespace SPP;

LogEntry LOG_APP("APP");

class TestConnection : public NetworkConnection
{
protected:


public:
	TestConnection(std::shared_ptr< Interface_PeerConnection > InPeer, bool IsServer) : NetworkConnection(InPeer, IsServer)
	{
	}

	virtual void Tick() override
	{

		NetworkConnection::Tick();
	}

	virtual void MessageReceived(const void* Data, int32_t DataLength)
	{
		
	}
};


using namespace std::chrono_literals;

int main(int argc, char* argv[])
{
	IntializeCore(nullptr);

	// START OS NETWORKING
	GetOSNetwork();

	bool bIsServer = true;

	for (int i = 0; i < argc; ++i)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "CC(%d):%s", i, argv[i]);

		auto Arg = std::string(argv[i]);
		if (Arg == "-ISCLIENT")
		{
			bIsServer = false;
			break;
		}
	}

	if (bIsServer)
	{
		//CreateChildProcess("networkTestd.exe", "-ISCLIENT");
	}

	SPP_LOG(LOG_APP, LOG_INFO, "NETTEST SERVER %d", bIsServer);

	std::shared_ptr< UDPSocket > _recvSocket;

	std::vector<uint8_t> recvBuffer;
	recvBuffer.resize(std::numeric_limits<uint16_t>::max());

	std::shared_ptr< TestConnection > Server;
	std::map< IPv4_SocketAddress, std::shared_ptr< TestConnection > > Clients;

	uint16_t PortNum = 33982;
	IPv4_SocketAddress serverAddr("127.0.0.1", PortNum);

#if USE_LIBDATACHANNEL
	std::shared_ptr< WebSocket > socketServer;
	if (bIsServer)
	{
		socketServer = std::make_shared< WebSocket >();
		socketServer->Listen(PortNum + 1);
	}
#endif


	if (bIsServer)
	{
		_recvSocket = std::make_shared<UDPSocket>(PortNum);
	}
	else
	{
		_recvSocket = std::make_shared<UDPSocket>();
		Server = std::make_shared< TestConnection >( std::make_shared<UDPSendWrapped>(_recvSocket, serverAddr), false);
		Server->SetPassword("yoyo");
		Server->Connect();
	}

	while (true)
	{
		IPv4_SocketAddress recvAddr;
		auto DataRecv = _recvSocket->ReceiveFrom(recvAddr, recvBuffer.data(), recvBuffer.size());

#if USE_LIBDATACHANNEL
		if (socketServer)
		{
			auto newConnection = socketServer->Accept();
			if(newConnection)
			{
				std::shared_ptr< TestConnection > currentClient = std::make_shared<TestConnection>(newConnection, true);
				Clients[newConnection->GetRemoteAddress()] = currentClient;
			}
		}
#endif

		if (DataRecv > 0)
		{
			if (bIsServer)
			{
				auto foundClient = Clients.find(recvAddr);
				std::shared_ptr< TestConnection > currentClient;

				if (foundClient == Clients.end())
				{
					currentClient = std::make_shared< TestConnection> (std::make_shared<UDPSendWrapped>(_recvSocket, recvAddr), true);
					currentClient->SetPassword("yoyo");
					Clients[recvAddr] = currentClient;
				}
				else
				{
					currentClient = foundClient->second;
				}

				currentClient->ReceivedRawData(recvBuffer.data(), DataRecv, 0);
			}
			else
			{
				if (recvAddr == serverAddr)
				{
					Server->ReceivedRawData(recvBuffer.data(), DataRecv, 0);
				}
			}
		}

		if (Server)
		{
			Server->Tick();
		}

		for (auto& [key, value] : Clients)
		{
			value->Tick();
		}

		std::this_thread::sleep_for(1ms);
	}

    return 0;
}
