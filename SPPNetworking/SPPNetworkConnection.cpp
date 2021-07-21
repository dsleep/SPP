// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPNetworkConnection.h"
#include "SPPSerialization.h"
#include "SPPLogging.h"
#include "SPPString.h"
#include "SPPNetworkMessenger.h"
#include "json/json.h"

namespace SPP
{
	LogEntry LOG_NETCON("NETCONN");

	#define MAGIC_HEADER_NUMBER 0x23FE

	struct ControlMessageHeader
	{
		uint16_t MAGIC_Num = MAGIC_HEADER_NUMBER;
		uint8_t IsControl = 0;
		uint16_t MessageLength = 0; //message size after header

		static int32_t BinarySize()
		{
			return sizeof(MAGIC_Num) + 
				sizeof(IsControl) + 
				sizeof(MessageLength);
		}

		std::string ToString()
		{
			return std::string_format("NUM: %u Control: %u MessageLength: %u", MAGIC_Num, IsControl, MessageLength);
		}
	};

	template<>
	inline BinarySerializer& operator<< <ControlMessageHeader>(BinarySerializer& StorageInterface, const ControlMessageHeader& Value)
	{
		StorageInterface << Value.MAGIC_Num;
		StorageInterface << Value.IsControl;
		StorageInterface << Value.MessageLength;
		return StorageInterface;
	}

	template<>
	inline BinarySerializer& operator>> <ControlMessageHeader>(BinarySerializer& StorageInterface, ControlMessageHeader& Value)
	{
		StorageInterface >> Value.MAGIC_Num;
		StorageInterface >> Value.IsControl;
		StorageInterface >> Value.MessageLength;
		return StorageInterface;
	}
		
	static const char *NetStateStrings[] =
	{ 
		"PENDING",
		"SAYING_HI",
		"RECV_WELCOME",
		"AUTHENTICATE",
		"CONNECTED",
		"DISCONNECTED"
	};

	NetworkConnection::NetworkConnection(std::shared_ptr< Interface_PeerConnection > InPeer) : _peerLink(InPeer)
	{
		_localGUID = std::generate_hex(16);
		_lastKeepAlive = std::chrono::steady_clock::now();

		_PingTimer.Initialize(
			[&]()
			{
				_SendState();
			},
			//TODO: MAKE this in chrono time... 500ms ...
				500
				);
	}

	void NetworkConnection::CloseDown(const char *Reason)
	{
		SPP_LOG(LOG_NETCON, LOG_WARNING, "NetworkConnection::CloseDown REASON: %s", Reason);
		// send close
		_NetworkState = EConnectionState::DISCONNECTED;
		_outGoingStream.Seek(0);
		_outGoingStream.GetArray().resize(0);
	}

	bool NetworkConnection::IsValid() const
	{
		return _NetworkState != EConnectionState::DISCONNECTED;
	}

	void NetworkConnection::_SendState()
	{
		_PingTimer.Reset();

		switch (_NetworkState)
		{
		case EConnectionState::SAYING_HI:
			_SendGenericMessage("Hello");
			break;
		case EConnectionState::RECV_WELCOME:
			_SendGenericMessage("Welcome");
			break;
		case EConnectionState::CONNECTED:
			_SendGenericMessage("Ping");
			break;
		default:
			//do nothing for others...
			break;
		}
	}

	//
	void NetworkConnection::_SendJSONConstrolString(const std::string &InValue)
	{
		// it is a control message
		ControlMessageHeader Header;
		Header.IsControl = 1;
		BinaryBlobSerializer MessageData;
		MessageData << Header;

		// get current offset
		int64_t CurrentPos = MessageData.Tell();
		MessageData << InValue;

		//TODO MAKE SURE UNDER 65k?

		// set new length and rewrite
		Header.MessageLength = (uint16_t)(MessageData.Size() - CurrentPos);
		MessageData.Seek(0);
		MessageData << Header;

		_AppendToOutoing(MessageData, (uint16_t)MessageData.Size());
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="Message"></param>
	void NetworkConnection::_SendGenericMessage(const char *Message)
	{
		SPP_LOG(LOG_NETCON, LOG_VERBOSE, "GUID: %s SendGenericMessage: %s", _localGUID.c_str(), Message);

		Json::Value JsonMessage;

		JsonMessage["Message"] = Message;
		JsonMessage["GUID"] = _localGUID;

		Json::StreamWriterBuilder wbuilder;
		std::string StrMessage = Json::writeString(wbuilder, JsonMessage);

		_SendJSONConstrolString(StrMessage);
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="ControlMsg"></param>
	void NetworkConnection::ProcessControlMessages(const std::string& ControlMsg)
	{
		SPP_LOG(LOG_NETCON, LOG_VERBOSE, "ProcessControlMessages: %s", ControlMsg.c_str());

		Json::Value root;
		Json::CharReaderBuilder Builder;
		Json::CharReader* reader = Builder.newCharReader();
		std::string Errors;

		bool parsingSuccessful = reader->parse(ControlMsg.c_str(), ControlMsg.c_str() + ControlMsg.length(), &root, &Errors);
		delete reader;
		if (!parsingSuccessful)
		{
			return;
		}

		Json::Value Message = root.get("Message", Json::Value::nullSingleton());
		Json::Value RemoteGUIDValue = root.get("GUID", Json::Value::nullSingleton());

		if (Message.isNull() == false)
		{
			std::string CheckMessage = Message.asCString();

			if (CheckMessage == "Hello")
			{
				_SendGenericMessage("Welcome");
				_SetState(EConnectionState::CONNECTED);
			}
			else if (CheckMessage == "Welcome")
			{
				_SetState(EConnectionState::CONNECTED);
			}
			else if (CheckMessage == "Ping")
			{
				if (IsServer() && _NetworkState != EConnectionState::CONNECTED)
				{
					_SendGenericMessage("Disconnect");
				}
				else if (_NetworkState == EConnectionState::CONNECTED)
				{
					_SendGenericMessage("Pong");
				}
			}
			else if (CheckMessage == "Pong")
			{
				_lastKeepAlive = std::chrono::steady_clock::now();
			}
			else if (CheckMessage == "Disconnect")
			{
				CloseDown("Disconnect Message Received");
			}
		}
	}


	void NetworkConnection::Connect()
	{
		_bIsServer = false;
		_SetState(EConnectionState::SAYING_HI);
	}

	void NetworkConnection::_SetState(EConnectionState InState)
	{
		SE_ASSERT((uint8_t)InState < ARRAY_SIZE(NetStateStrings));
		SPP_LOG(LOG_NETCON, LOG_WARNING, "GUID: %s SetState: %s", _localGUID.c_str(), NetStateStrings[(uint8_t)InState]);

		_NetworkState = InState;
		_SendState();
	}

	// connection state
	void NetworkConnection::Tick()
	{
		if (_NetworkState == EConnectionState::DISCONNECTED)
		{
			return;
		}

		auto CurrentTime = std::chrono::steady_clock::now();

		_PingTimer.Poll();

		if (_NetworkState < EConnectionState::CONNECTED)
		{
			// DC'd
			if (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - _lastKeepAlive).count() > 15)
			{
				CloseDown("time out while trying to connect");
				return;
			}
		}
		else
		{
			// DC'd
			if (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - _lastKeepAlive).count() > 10)
			{
				CloseDown("time out during active connection");
				return;
			}

			for (auto &coder : _Transcoders)
			{
				coder->Update(this);
			}
		}

		// send it every tick anyway
		_ImmediatelySendOutGoing();
		_UpdateNetworkFlow();
	}

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	int32_t NetworkConnection::GetBufferedAmount() const
	{
		int32_t bufferedAmount = 0;

		for (auto &coder : _Transcoders)
		{
			bufferedAmount += coder->GetBufferedAmount();
		}

		bufferedAmount += _outGoingStream.Size();
		bufferedAmount += _incomingStream.Size();

		return bufferedAmount;
	}

	int32_t NetworkConnection::GetBufferedMessageCount() const
	{
		int32_t bufferedCount = 0;

		for (auto& coder : _Transcoders)
		{
			bufferedCount += coder->GetBufferedMessageCount();
		}

		return bufferedCount;
	}	

	/// <summary>
	/// 
	/// </summary>
	/// <param name="OutgoingDataLimitInBytes"></param>
	void NetworkConnection::SetOutgoingByteRate(int32_t OutgoingDataLimitInBytes)
	{
		SPP_LOG(LOG_NETCON, LOG_INFO, "Setting %s outgoing limit %d KB/s", ToString().c_str(), OutgoingDataLimitInBytes / 1024);
				
		_settings.maxPerSecondRateLimitInBytes = (float)OutgoingDataLimitInBytes;
		_settings.CCPerSecondRateLimitInBytes = std::min(_settings.CCPerSecondRateLimitInBytes, _settings.maxPerSecondRateLimitInBytes);
	}

	/// <summary>
	/// 
	/// </summary>
	void NetworkConnection::DegradeConnection()
	{
		_bDegrade = true;
		_degradeStartTime = HighResClock::now();
	}

	/// <summary>
	/// 
	/// </summary>
	void NetworkConnection::_UpdateNetworkFlow()
	{
		auto CurrentTime = HighResClock::now();
				
		float LastUpdateTimeInSeconds = (float)std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - _stats.LastUpdateTime).count() / 1000.0f;
		float LastDataCalcTime = (float)std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - _stats.LastCalcTime).count() / 1000.0f;
		
		if (_bDegrade)
		{			
			float DegradeTimeInSeconds = (float)std::chrono::duration_cast<std::chrono::milliseconds>(CurrentTime - _degradeStartTime).count() / 1000.0f;			
			_settings.RateAcceleration = -100000;
			_settings.RateVelocity = std::min<float>(_settings.RateVelocity, 0);
			if (DegradeTimeInSeconds > 2.0f)
			{
				_bDegrade = false;
			}
		}
		else
		{
			_settings.RateAcceleration = 100;
		}

		_settings.RateVelocity += _settings.RateAcceleration * LastUpdateTimeInSeconds;
		_settings.RateVelocity = std::clamp(_settings.RateVelocity, -3 * 1024.0f * 1024.0f, 500 * 1024.0f);
		_settings.CCPerSecondRateLimitInBytes += _settings.RateVelocity * LastUpdateTimeInSeconds;

		// 1KB/s minimum
		_settings.CCPerSecondRateLimitInBytes = std::clamp(_settings.CCPerSecondRateLimitInBytes, 1 * 1024.0f, _settings.maxPerSecondRateLimitInBytes);

		if (LastDataCalcTime > 1.0f)
		{
			_stats.LastKBsOutgoing = (_stats.CurrentOutgoingAmount / LastDataCalcTime) / 1024.0f;
			_stats.LastKBsIncoming = (_stats.CurrentIncomingAmount / LastDataCalcTime) / 1024.0f;
			
			_stats.CurrentOutgoingAmount = 0;
			_stats.CurrentIncomingAmount = 0;

			_stats.LastCalcTime = CurrentTime;
			_stats.LastCalcTimeFromAppStart = TimeSinceAppStarted();

			SPP_LOG(LOG_NETCON, LOG_INFO, "**STATUS REPORT:**");// , _remoteAddr.ToString().c_str());
			SPP_LOG(LOG_NETCON, LOG_INFO, " - State %s Total Packets Recv: %llu Total Packets Recv: %llu", 
				NetStateStrings[(uint8_t)_NetworkState], 
				_stats.TotalPKtsRcv,
				_stats.TotalPktsSnd);
			SPP_LOG(LOG_NETCON, LOG_INFO, " - outgoing %4.2f KB/s incoming %4.2f KB/s",
				_stats.LastKBsOutgoing, 
				_stats.LastKBsIncoming);

			for (auto &coder : _Transcoders)
			{
				coder->Report();
			}		

			//_SocketLink->Report();
		}

		_settings.outgoingRateControl -= (int32_t)(LastUpdateTimeInSeconds * _settings.CCPerSecondRateLimitInBytes);
		_settings.outgoingRateControl = std::max(_settings.outgoingRateControl, 0);

		if (_settings.outgoingRateControl < _settings.CCPerSecondRateLimitInBytes )
		{
			if (bIsSaturated)
			{
				SPP_LOG(LOG_NETCON, LOG_WARNING, "NetworkConnection: IS NOT LONGER SATURATED", ToString().c_str());
			}
			bIsSaturated = false;
		}

		_stats.LastUpdateTime = CurrentTime;		
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="Data"></param>
	/// <param name="DataLength"></param>
	void NetworkConnection::_AppendToOutoing(const void *Data, uint16_t DataLength)
	{
		if (_NetworkState == EConnectionState::DISCONNECTED)
		{
			return;
		}

		if (_outGoingStream.Size() + DataLength > IDEAL_NETWORK_MESSAGE_SIZE)
		{
			_ImmediatelySendOutGoing();
		}

		_outGoingStream.Write(Data, DataLength);
	}

	/// <summary>
	/// 
	/// </summary>
	void NetworkConnection::_ImmediatelySendOutGoing()
	{
		// socket send
		if (_outGoingStream.Size() > 0)
		{			
			_stats.CurrentOutgoingAmount += _outGoingStream.Size();
			_settings.outgoingRateControl += _outGoingStream.Size();

			if (_settings.outgoingRateControl > _settings.CCPerSecondRateLimitInBytes)
			{
				if (bIsSaturated == false)
				{
					SPP_LOG(LOG_NETCON, LOG_WARNING, "NetworkConnection: IS SATURATED", ToString().c_str());
				}
				bIsSaturated = true;				
			}	

			_peerLink->Send(_outGoingStream, (uint16_t)_outGoingStream.Size());
			_stats.TotalPktsSnd++;
			
			_outGoingStream.Seek(0);
			_outGoingStream.GetArray().resize(0);
		}
	}

	void NetworkConnection::_RawSend(const void *buf, uint16_t DataLength)
	{
		_AppendToOutoing(buf, DataLength);
	}

	/////////////////////////
	// comes from external calls...
	void NetworkConnection::SendMessage(const void *Data, int32_t DataLength, uint16_t MsgMaskInfo)
	{
		// must have transcoders
		SE_ASSERT(_Transcoders.size() > 0, RR_HARD_ASSERT);
		_Transcoders.back()->Send(this, Data, DataLength, MsgMaskInfo);
	}
	void NetworkConnection::ReceivedRawData(const void *Data, int32_t DataLength, uint16_t MsgMaskInfo)
	{
		_stats.CurrentIncomingAmount += DataLength;

		// transcoder recv goes other way from netconnection down to last transcoder
		if (_NetworkState == EConnectionState::DISCONNECTED)
		{
			return;
		}

		// lowest level of receive, raw bits from socket
		// decide if its a base control message or to pass to transcoder

		//make this a circuluar ring
		_incomingStream.Seek(_incomingStream.Size());
		_incomingStream.Write(Data, DataLength);

		while (_incomingStream.Size() >= sizeof(ControlMessageHeader))
		{
			_incomingStream.Seek(0);

			MemoryView DataView(_incomingStream, _incomingStream.Size());

			ControlMessageHeader Header;
			DataView >> Header;

			//check endian here?
			if (Header.MAGIC_Num != MAGIC_HEADER_NUMBER)
			{
				SPP_LOG(LOG_NETCON, LOG_WARNING, "CONTROL HEADER: %s", Header.ToString().c_str());
				CloseDown("bad magic number!");
				return;
			}

			// double check super big header
			if (Header.MessageLength > 40 * 1024)
			{
				SPP_LOG(LOG_NETCON, LOG_WARNING, "CONTROL HEADER: %s", Header.ToString().c_str());
				CloseDown("weirdly large message header");
				return;
			}

			//TODO make sure this is valid size?
			int64_t Remaining = (DataView.Size() - DataView.Tell());
			if (Remaining >= Header.MessageLength)
			{
				DataView.RebuildViewSentLength(Header.MessageLength);

				if (Header.IsControl)
				{
					std::string ControlMsg;
					DataView >> ControlMsg;
					ProcessControlMessages(ControlMsg);
				}
				// if we are not connected do not process any lower!!!
				else if (_NetworkState == EConnectionState::CONNECTED)
				{
					//SPP_LOG(LOG_NETCON, LOG_INFO, "Recv Non Control Size: %d", DataView.Size());
					// use a view to make things a little more clear	
					_Transcoders.front()->Recv( this, DataView, (int32_t)DataView.Size(), MsgMaskInfo);
				}

				// message consumed
				std::vector<uint8_t>& IncomingData = _incomingStream.GetArray();
				IncomingData.erase(IncomingData.begin(), IncomingData.begin() + ControlMessageHeader::BinarySize() + Header.MessageLength);
				_incomingStream.Seek(0);

				_stats.TotalPKtsRcv++;
			}
			else
			{
				break;
			}
		}
	}

	// should only come from transcoding...
	void NetworkConnection::Send(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo)	
	{
		SE_ASSERT(this == InConnection);

		ControlMessageHeader Header;
		Header.MessageLength = DataLength;

		BinaryBlobSerializer MessageData;
		MessageData << Header;
		MessageData.Write(Data, DataLength);

		_AppendToOutoing(MessageData, (uint16_t)MessageData.Size());
	}

	// should only come from transcoding...
	void NetworkConnection::Recv(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo)
	{
		SE_ASSERT(this == InConnection);
		MessageReceived(Data, DataLength);
	}
}
