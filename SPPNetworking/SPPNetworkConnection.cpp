// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPNetworkConnection.h"
#include "SPPSerialization.h"
#include "SPPLogging.h"
#include "SPPString.h"
#include "SPPNetworkMessenger.h"
#include "SPPJsonUtils.h"

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
			return std::string_format("NUM: 0x%X Control: %u MessageLength: %u", MAGIC_Num, IsControl, MessageLength);
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
		"UNKNOWN",
		"S_WAITING",
		"S_SENDING_PUBLIC_KEY",

		"C_SAYING_HI",
		"C_SENDING_SHARED_KEY",

		"AUTHENTICATE_PASSWORD",
		"CONNECTED",
		"DISCONNECTED"
	};

	NetworkConnection::NetworkConnection(std::shared_ptr< Interface_PeerConnection > InPeer, bool bIsServer) : _peerLink(InPeer), _bIsServer(bIsServer)
	{
		static_assert(ARRAY_SIZE(NetStateStrings) == (uint8_t)EConnectionState::STATE_COUNT);

		_localGUID = std::generate_hex(16);
		_lastKeepAlive = std::chrono::steady_clock::now();

#if SPP_NETCONN_CRYPTO
		_rsaCipherLocal.GenerateKeyPair(1024);

		if (!_bIsServer)
		{
			_aesCipherShared.GenerateKey();
		}
#endif

		if (_bIsServer)
		{
			_networkState = EConnectionState::S_WAITING;
		}

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
		if (_networkState != EConnectionState::DISCONNECTED)
		{
			_SendGenericMessage(std::string_format("Disconnect: %s", Reason).c_str());
			_ImmediatelySendOutGoing();
			_SetState(EConnectionState::DISCONNECTED);

			SPP_LOG(LOG_NETCON, LOG_WARNING, "NetworkConnection::CloseDown REASON: %s", Reason);
		}
	}

	bool NetworkConnection::IsValid() const
	{
		return _networkState != EConnectionState::DISCONNECTED;
	}

	bool NetworkConnection::IsConnected() const
	{
		return _networkState == EConnectionState::CONNECTED;
	}
	
	void NetworkConnection::_SendState()
	{
		_PingTimer.Reset();

		switch (_networkState)
		{
		case EConnectionState::S_WAITING:
		case EConnectionState::S_SENDING_PUBLIC_KEY:
		case EConnectionState::C_SAYING_HI:
		case EConnectionState::C_SENDING_SHARED_KEY:
		case EConnectionState::AUTHENTICATE_PASSWORD:
			_SendGenericMessage("STATEFUL");
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
	void NetworkConnection::_SendJSONConstrolMessage(const std::vector<uint8_t>& Data, bool Encrypted)
	{
		// it is a control message
		ControlMessageHeader Header;
		Header.IsControl = 1;
		BinaryBlobSerializer MessageData;
		MessageData << Header;

		// get current offset
		int64_t CurrentPos = MessageData.Tell();
		MessageData << (uint8_t)Encrypted;
		MessageData << Data;

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
		JsonMessage["State"] = (uint8_t)_networkState;
		JsonMessage["GUID"] = _localGUID;

#if SPP_NETCONN_CRYPTO
		if (_networkState < EConnectionState::AUTHENTICATE_PASSWORD)
		{
			JsonMessage["RSA"] = _rsaCipherLocal.GetPublicKey();

			if (!_bIsServer && _rsaCipherRemote.CanEncrypt())
			{
				JsonMessage["AES"] = _rsaCipherRemote.EncryptString(_aesCipherShared.GetKey());
			}
		}
		else if (_networkState == EConnectionState::AUTHENTICATE_PASSWORD)
		{
			if (!_bIsServer)
			{
				JsonMessage["PASSWORD"] = _serverPassword;
			}
		}
				
#endif

		bool bIsEncrypted = false;
		Json::StreamWriterBuilder wbuilder;
		std::string StrMessage = Json::writeString(wbuilder, JsonMessage);

		std::vector<uint8_t> oData;

#if SPP_NETCONN_CRYPTO		
		if (_networkState == EConnectionState::AUTHENTICATE_PASSWORD ||
			_networkState == EConnectionState::CONNECTED)
		{
			bIsEncrypted = true;
			EncryptData(StrMessage.data(), StrMessage.size(), oData);
		}		
		else
		{
			oData.insert(oData.begin(), StrMessage.begin(), StrMessage.end());
		}
#else
		oData.insert(oData.begin(), StrMessage.begin(), StrMessage.end());
#endif

		_SendJSONConstrolMessage(oData, bIsEncrypted);
	}

	void NetworkConnection::CLIENT_ProcessControlMessages(Json::Value &jsonMessage)
	{
		Json::Value Message = jsonMessage.get("Message", Json::Value::nullSingleton());
		Json::Value RemoteState = jsonMessage.get("State", Json::Value::nullSingleton());
		Json::Value RSAValue = jsonMessage.get("RSA", Json::Value::nullSingleton());
		Json::Value AESValue = jsonMessage.get("AES", Json::Value::nullSingleton());
		Json::Value RemoteGUIDValue = jsonMessage.get("GUID", Json::Value::nullSingleton());

		if (RemoteState.isNull() || Message.isNull())
		{
			return;
		}

		EConnectionState msgState = (EConnectionState)RemoteState.asUInt();
		std::string MessageString = Message.asCString();
				
		if (StartsWith(MessageString, "Disconnect"))
		{
			CloseDown(MessageString.c_str());
			return;
		}

		// using our state we should know the exact flow
		switch (_networkState)
		{
#if SPP_NETCONN_CRYPTO
		case EConnectionState::C_SAYING_HI:
			if (!RSAValue.isNull())
			{
				std::string RSAString = RSAValue.asCString();
				_rsaCipherRemote.SetPublicKey(RSAString);
				SPP_LOG(LOG_NETCON, LOG_WARNING, "Received Public RSA Key: %s", RSAString.c_str());
				_SetState(EConnectionState::C_SENDING_SHARED_KEY);
			}
			break;
		case EConnectionState::C_SENDING_SHARED_KEY:
			if (msgState == EConnectionState::AUTHENTICATE_PASSWORD)
			{
				_SetState(EConnectionState::AUTHENTICATE_PASSWORD);
			}
			break;
		case EConnectionState::AUTHENTICATE_PASSWORD:
			if (msgState == EConnectionState::CONNECTED)
			{
				_SetState(EConnectionState::CONNECTED);
			}
			break;
#else
		case EConnectionState::C_SAYING_HI:
			if (msgState == EConnectionState::CONNECTED)
			{
				_SetState(EConnectionState::CONNECTED);
			}
			break;
#endif
		case EConnectionState::CONNECTED:
			if (MessageString == "Ping")
			{
				_SendGenericMessage("Pong");
			}
			else if (MessageString == "Pong")
			{
				_lastKeepAlive = std::chrono::steady_clock::now();
			}
			break;
		}
	}

	void NetworkConnection::SERVER_ProcessControlMessages(Json::Value& jsonMessage)
	{		
		Json::Value Message = jsonMessage.get("Message", Json::Value::nullSingleton());
		Json::Value RemoteState = jsonMessage.get("State", Json::Value::nullSingleton());
		Json::Value RSAValue = jsonMessage.get("RSA", Json::Value::nullSingleton());
		Json::Value AESValue = jsonMessage.get("AES", Json::Value::nullSingleton());
		Json::Value RemoteGUIDValue = jsonMessage.get("GUID", Json::Value::nullSingleton());
		Json::Value PasswordValue = jsonMessage.get("PASSWORD", Json::Value::nullSingleton());
		
		if (RemoteState.isNull() || Message.isNull())
		{
			return;
		}

		EConnectionState msgState = (EConnectionState)RemoteState.asUInt();
		std::string MessageString = Message.asCString();

		if (MessageString == "Disconnect")
		{
			CloseDown("Disconnect Message Received");
			return;
		}

		// using our state we should know the exact flow
		switch (_networkState)
		{
#if SPP_NETCONN_CRYPTO
		case EConnectionState::S_WAITING:
			if (!RSAValue.isNull())
			{
				std::string RSAString = RSAValue.asCString();
				_rsaCipherRemote.SetPublicKey(RSAString);
				SPP_LOG(LOG_NETCON, LOG_WARNING, "Received Public RSA Key: %s", RSAString.c_str());
				_SetState(EConnectionState::S_SENDING_PUBLIC_KEY);
			}
			break;
		case EConnectionState::S_SENDING_PUBLIC_KEY:
			if (!AESValue.isNull())
			{
				std::string AESString = AESValue.asCString();
				AESString = _rsaCipherLocal.DecryptString(AESString);
				SPP_LOG(LOG_NETCON, LOG_WARNING, "Received AES Key: %s", AESString.c_str());
				_aesCipherShared.SetKey(AESString);
				_SetState(EConnectionState::AUTHENTICATE_PASSWORD);
			}			
			break;
		case EConnectionState::AUTHENTICATE_PASSWORD:
			if (!PasswordValue.isNull())
			{
				std::string PasswordString = PasswordValue.asCString();

				SPP_LOG(LOG_NETCON, LOG_WARNING, "Checking Password %s to %s", PasswordString.c_str(), _serverPassword.c_str());

				if (PasswordValue == _serverPassword)
				{		
					SPP_LOG(LOG_NETCON, LOG_WARNING, " - PASSWORD VALID");
					_SetState(EConnectionState::CONNECTED);
				}
				else
				{
					CloseDown("Invalid Password");
				}
			}
			break;
#else
		case EConnectionState::S_WAITING:
			if (MessageString == "C_SAYING_HI")
			{
				_SetState(EConnectionState::CONNECTED);
			}
			break;
#endif
		case EConnectionState::CONNECTED:
			if (MessageString == "Ping")
			{
				_SendGenericMessage("Pong");
			}
			else if (MessageString == "Pong")
			{
				_lastKeepAlive = std::chrono::steady_clock::now();
			}
			break;
		}
	}

#if SPP_NETCONN_CRYPTO
	void NetworkConnection::EncryptData(const void* InData, size_t DataLength, std::vector<uint8_t>& oData)
	{
		_aesCipherShared.EncryptData(InData, DataLength, oData);
	}
	void NetworkConnection::DecryptData(const void* InData, size_t DataLength, std::vector<uint8_t>& oData)
	{
		_aesCipherShared.DecryptData(InData, DataLength, oData);
	}
#endif

	/// <summary>
	/// 
	/// </summary>
	/// <param name="ControlMsg"></param>
	void NetworkConnection::ProcessControlMessages(const std::vector<uint8_t>& ControlMsg, bool Encrypted)
	{
		Json::Value jsonMessage;

		if (Encrypted)
		{
#if SPP_NETCONN_CRYPTO			
			std::vector<uint8_t> oData;
			DecryptData(ControlMsg.data(), ControlMsg.size(), oData);
			if (MemoryToJson(oData.data(), oData.size(), jsonMessage) == false)
			{
				SPP_LOG(LOG_NETCON, LOG_INFO, "FAILED TO PARSE JSON");
				return;
			}
#else	
			SE_ASSERT(0);//can't decrypt
#endif
		}
		else
		{
#if SPP_NETCONN_CRYPTO		
			if (_networkState == EConnectionState::CONNECTED)
			{
				SPP_LOG(LOG_NETCON, LOG_INFO, "HAS NON Encrypted CONTROL?!");
				return;
			}
#endif

			if (MemoryToJson(ControlMsg.data(), ControlMsg.size(), jsonMessage) == false)
			{
				SPP_LOG(LOG_NETCON, LOG_INFO, "FAILED TO PARSE JSON");
				return;
			}
		}

		if (_bIsServer)SERVER_ProcessControlMessages(jsonMessage);
		else CLIENT_ProcessControlMessages(jsonMessage);
	}


	void NetworkConnection::Connect()
	{
		_SetState(EConnectionState::C_SAYING_HI);
	}

	void NetworkConnection::_SetState(EConnectionState InState)
	{
		SE_ASSERT((uint8_t)InState < ARRAY_SIZE(NetStateStrings));
		
		if (_networkState != InState)
		{
			SPP_LOG(LOG_NETCON, LOG_INFO, "GUID: %s SetState: %s from %s", 
				_localGUID.c_str(),
				NetStateStrings[(uint8_t)InState], 
				NetStateStrings[(uint8_t)_networkState]);
			_networkState = InState;
		}
		
		_SendState();
	}

	// connection state
	void NetworkConnection::Tick()
	{
		if (_networkState == EConnectionState::DISCONNECTED)
		{
			return;
		}

		auto CurrentTime = std::chrono::steady_clock::now();
			
		if (_networkState < EConnectionState::CONNECTED)
		{
			_SendState();

			// DC'd
			if (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - _lastKeepAlive).count() > 10)
			{
				CloseDown("time out while trying to connect");
				return;
			}
		}
		else
		{
			_PingTimer.Poll();

			// DC'd
			if (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - _lastKeepAlive).count() > 5)
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

		bufferedAmount += (int32_t)_outGoingStream.Size();
		bufferedAmount += (int32_t)_incomingStream.Size();

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

			if (bReportStatus)
			{
				SPP_LOG(LOG_NETCON, LOG_INFO, "**STATUS REPORT:**");// , _remoteAddr.ToString().c_str());
				SPP_LOG(LOG_NETCON, LOG_INFO, " - State %s Total Packets Recv: %llu Total Packets Recv: %llu",
					NetStateStrings[(uint8_t)_networkState],
					_stats.TotalPKtsRcv,
					_stats.TotalPktsSnd);
				SPP_LOG(LOG_NETCON, LOG_INFO, " - outgoing %4.2f KB/s incoming %4.2f KB/s",
					_stats.LastKBsOutgoing,
					_stats.LastKBsIncoming);

				for (auto& coder : _Transcoders)
				{
					coder->Report();
				}
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
		if (_networkState == EConnectionState::DISCONNECTED)
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
		SE_ASSERT(_Transcoders.size() > 0);
		_Transcoders.back()->Send(this, Data, DataLength, MsgMaskInfo);
	}
	void NetworkConnection::ReceivedRawData(const void *Data, int32_t DataLength, uint16_t MsgMaskInfo)
	{
		_stats.CurrentIncomingAmount += DataLength;

		// transcoder recv goes other way from netconnection down to last transcoder
		if (_networkState == EConnectionState::DISCONNECTED)
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
					uint8_t encrypted = 0;
					std::vector<uint8_t> ControlMsg;					
					DataView >> encrypted;
					DataView >> ControlMsg;
					ProcessControlMessages(ControlMsg, encrypted);
				}
				// if we are not connected do not process any lower!!!
				else if (_networkState == EConnectionState::CONNECTED)
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
