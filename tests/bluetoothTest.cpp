// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPCore.h"
#include "json/json.h"
#include "SPPLogging.h"
#include <set>

#include "SPPMemory.h"
#include "SPPPlatformCore.h"

#include "SPPSockets.h"

#include "SPPWinRTBTE.h"

using namespace SPP;

LogEntry LOG_APP("APP");

static std::vector<uint8_t> startMessage = { 0, 1, 2, 3 };
static std::vector<uint8_t> endMessage = { 3, 2, 1, 0 };

class SimpleBTConnection
{
protected:
	std::vector<uint8_t> streamData;
	std::vector<uint8_t> recvBuffer;

	std::shared_ptr< Interface_PeerConnection > _peerLink;
public:
	SimpleBTConnection(std::shared_ptr< Interface_PeerConnection > InPeer) : _peerLink(InPeer)
	{
	}

	bool IsValid()
	{
		if (_peerLink)
		{
			return true;
		}
		return false;
	}

	void Tick() 
	{
		if (_peerLink)
		{
			if (_peerLink->IsBroken())
			{
				SPP_LOG(LOG_APP, LOG_INFO, "PEER LINK BROKEN");
				_peerLink.reset();
				return;
			}
		}
		else
		{
			return;
		}

		recvBuffer.resize(std::numeric_limits<uint16_t>::max());
		auto DataRecv = _peerLink->Receive(recvBuffer.data(), recvBuffer.size());
		if (DataRecv > 0)
		{
			SPP_LOG(LOG_APP, LOG_INFO, "GOT BT DATA: %d", DataRecv);

			streamData.insert(streamData.end(), recvBuffer.begin(), recvBuffer.begin() + DataRecv);

			auto FindStart = std::search(streamData.begin(), streamData.end(), startMessage.begin(), startMessage.end());

			if (FindStart != streamData.end())
			{
				auto FindEnd = std::search(FindStart, streamData.end(), endMessage.begin(), endMessage.end());

				if (FindEnd != streamData.end())
				{
					std::string messageString(FindStart + startMessage.size(), FindEnd);
					MessageReceived(messageString);
					streamData.clear();
				}
			}

			if (streamData.size() > 500)
			{
				streamData.clear();
			}
		}
	}

	void MessageReceived(const std::string &InMessage)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "MessageReceived: %s", InMessage.c_str());
	}
};


using namespace std::chrono_literals;


int main(int argc, char* argv[])
{
	IntializeCore(nullptr);

	// START OS NETWORKING
	GetOSNetwork();

	auto dataOne = [](uint8_t* InData, size_t DataSize)
	{
		SPP_LOG(LOG_APP, LOG_INFO, "BTE data: %d", DataSize);
	};

	BTEWatcher watcher;
	watcher.WatchForData("366DEE95-85A3-41C1-A507-8C3E02342000",
		{ 
			{ "366DEE95-85A3-41C1-A507-8C3E02342001", dataOne }
		});

	while (true)
	{
		std::this_thread::sleep_for(10ms);
	}

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

	SPP_LOG(LOG_APP, LOG_INFO, "NETTEST SERVER %d", bIsServer);

	std::shared_ptr< BlueToothSocket > listenSocket;

	if (bIsServer)
	{
		listenSocket = std::make_shared<BlueToothSocket>();
		listenSocket->Listen();
	}
	else
	{
		//_recvSocket = std::make_shared<UDPSocket>();
		//Server = std::make_shared< BTConnection >(std::make_shared<BlueToothConnection>(_recvSocket, serverAddr), false);
		//Server->SetPassword("yoyo");
		//Server->Connect();
	}

	std::shared_ptr< SimpleBTConnection > singleConnection;
	while (true)
	{		
		if (singleConnection)
		{
			if (singleConnection->IsValid())
			{
				singleConnection->Tick();
			}
			else
			{
				singleConnection.reset();
			}			
		}
		else
		{
			auto newBTConnection = listenSocket->Accept();
			if (newBTConnection)
			{
				singleConnection = std::make_shared< SimpleBTConnection >(newBTConnection);
				SPP_LOG(LOG_APP, LOG_INFO, "HAS BLUETOOTH CONNECT");
			}
		}

		std::this_thread::sleep_for(1ms);
	}

	return 0;
}
