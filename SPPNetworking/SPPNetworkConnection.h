// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPNetworking.h"
#include "SPPSerialization.h"
#include <functional>
#include "SPPSockets.h"
#include "SPPTiming.h"
#include "SPPSTLUtils.h"
#include "SPPJsonUtils.h"

#if SPP_NETCONN_CRYPTO
	#include "SPPCrypto.h"
#endif

namespace SPP
{
	enum class EConnectionState : uint8_t
	{
		UNKNOWN = 0,

		S_WAITING = 1,
		S_SENDING_PUBLIC_KEY = 2,
		
		C_SAYING_HI = 3,
		C_SENDING_SHARED_KEY = 4,
		
		AUTHENTICATE_PASSWORD = 5,

		CONNECTED = 6,
		DISCONNECTED = 7,
		STATE_COUNT = 8
	};

	namespace EMessageMask
	{
		enum EVALUES : uint16_t
		{
			IS_RELIABLE = 1 << 1,
			UNUSED = 1 << 2,
			// ...
		};
	}
	
	class SPP_NETWORKING_API Interface_MessageTranscoder
	{
		friend class NetworkConnection;

	protected:
		std::weak_ptr < Interface_MessageTranscoder > _PreviousTranscoder;
		std::weak_ptr < Interface_MessageTranscoder > _NextTranscoder;

	public:
		virtual void Recv(class NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo) = 0;
		virtual void Send(class NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo) = 0;
		virtual void Update(class NetworkConnection *InConnection) { };
		virtual void Report() { };
		virtual std::string ReportString() { return "Interface_MessageTranscoder"; };
		virtual int32_t GetBufferedAmount() const { return 0; }
		virtual int32_t GetBufferedMessageCount() const { return 0; }
	};
		
	struct ConnectionStats
	{
		uint64_t TotalPktsSnd = 0;
		uint64_t TotalPKtsRcv = 0;
		uint64_t TotalBytesSnd = 0;
		uint64_t TotalBytesRcv = 0;

		float LastKBsOutgoing = 0.0;
		float LastKBsIncoming = 0.0;

		uint64_t CurrentIncomingAmount = 0;
		uint64_t CurrentOutgoingAmount = 0;

		HighResClock::time_point LastUpdateTime;
		HighResClock::time_point LastCalcTime;

		double LastCalcTimeFromAppStart = 0.0;
	};

	struct ConnectionSettings
	{
		static constexpr float DefaultMaxMegabitOutgoing = 100;

		float maxPerSecondRateLimitInBytes = ((DefaultMaxMegabitOutgoing / 8.0f) * 1024.0f * 1024.0f);
		float CCPerSecondRateLimitInBytes = ((DefaultMaxMegabitOutgoing / 8.0f) * 1024.0f * 1024.0f);

		float RateVelocity = 0.0f;
		float RateAcceleration = 0.0f;

		int32_t outgoingRateControl = 0;		
	};

	// this base layer comm is simple it pulses the message of its current state
	class SPP_NETWORKING_API NetworkConnection : public Interface_MessageTranscoder, public SPP::inheritable_enable_shared_from_this<NetworkConnection>
	{
		using HighResClock = std::chrono::high_resolution_clock;

	protected:

#if SPP_NETCONN_CRYPTO
		RSA_Cipher _rsaCipherLocal;
		RSA_Cipher _rsaCipherRemote;

		AES_Cipher _aesCipherShared;
#endif

		bool _bIsServer = true;
		bool _bNeedsEndianSwap = false;

		EConnectionState _networkState = EConnectionState::UNKNOWN;
		EConnectionState _remoteState = EConnectionState::UNKNOWN;

		ConnectionStats _stats;
		ConnectionSettings _settings;

		std::shared_ptr< Interface_PeerConnection > _peerLink;
		std::list< std::shared_ptr< Interface_MessageTranscoder > > _Transcoders;

		std::chrono::steady_clock::time_point _lastKeepAlive;
		SimplePolledRepeatingTimer< std::chrono::milliseconds > _PingTimer;

		BinaryBlobSerializer _outGoingStream;
		BinaryBlobSerializer _incomingStream;

		std::string _serverPassword;

		std::string _localGUID;
		std::string _remoteGUID;	

		bool _bDegrade = false;
		HighResClock::time_point _degradeStartTime;

		bool bIsSaturated = false;
		bool bReportStatus = false;

		//internal
		void _UpdateNetworkFlow();
		void _AppendToOutoing(const void* Data, uint16_t DataLength);
		void _ImmediatelySendOutGoing();
		void _SendJSONConstrolMessage(const std::vector<uint8_t> &Data, bool Encrypted);
		void _SendGenericMessage(const char* Message);

		void _SendState();
		void _SetState(EConnectionState InState);
		
		virtual void _RawSend(const void* buf, uint16_t DataLength);

	public:
		NetworkConnection(std::shared_ptr< Interface_PeerConnection > InPeer, bool bIsServer);
		virtual ~NetworkConnection() = default;

		bool IsServer() const
		{
			return _bIsServer;
		}

		std::string ToString() const
		{
			//SE_ASSERT(_peerLink);
			//return _remoteAddr.ToString();
			return "";
		}
				
		std::list< std::shared_ptr< Interface_MessageTranscoder > > GetTranscoderStack()
		{
			return _Transcoders;
		}
		
		template<typename... Args>
		void CreateTranscoderStack(Args... args)
		{
			_Transcoders.clear();

			//Network connection is both the first and last of the transcoder cyclic stack, but not in the stack itself
			std::shared_ptr< Interface_MessageTranscoder > CurrentTranscoder = shared_from_this();

			auto loop = [&](auto && Arg)
			{
				CurrentTranscoder->_NextTranscoder = Arg;

				_Transcoders.push_back(Arg);

				Arg->_PreviousTranscoder = CurrentTranscoder;
				CurrentTranscoder = Arg;
			};
			
			(loop(args), ...);

			//Network connection is both the first and last of the transcoder cyclic stack, but not in the stack itself
			_Transcoders.back()->_NextTranscoder = shared_from_this();
		}

		void SetPassword(const std::string InPassword)
		{
			_serverPassword = InPassword;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		const ConnectionStats &GetStats() const
		{
			return _stats;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		const ConnectionSettings&GetSettings() const
		{
			return _settings;
		}
		
		/// <summary>
		/// 
		/// </summary>
		void Connect();
		void CloseDown(const char *Reason);

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		bool IsValid() const;

		bool IsConnected() const;
		
		// connection state
		virtual void Tick();
		
		void SERVER_ProcessControlMessages(Json::Value& jsonMessage);
		void CLIENT_ProcessControlMessages(Json::Value& jsonMessage);
		void ProcessControlMessages(const std::vector<uint8_t>& ControlMsg, bool Encrypted);

		virtual int32_t GetBufferedAmount() const override;
		virtual int32_t GetBufferedMessageCount() const override;


#if SPP_NETCONN_CRYPTO
		void EncryptData(const void* InData, size_t DataLength, std::vector<uint8_t>& oData);
		void DecryptData(const void* InData, size_t DataLength, std::vector<uint8_t>& oData);
#endif

		bool IsSaturated() const {
			return bIsSaturated;
		}

		void SetOutgoingByteRate(int32_t OutgoingDataLimitInBytes);
		void DegradeConnection();
	
		////////////////////////
		//TRANSCODER INTERFACE
		// should only come from transcoding...
		virtual void Send(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo);
		// something higher up is passing down data
		virtual void Recv(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo);
		////////////////////////

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Data"></param>
		/// <param name="DataLength"></param>
		/// <param name="MsgMaskInfo"></param>
		virtual void ReceivedRawData(const void* Data, int32_t DataLength, uint16_t MsgMaskInfo);

		/// <summary>
		/// /
		/// </summary>
		/// <param name="Data"></param>
		/// <param name="DataLength"></param>
		/// <param name="MsgMaskInfo"></param>
		virtual void SendMessage(const void* Data, int32_t DataLength, uint16_t MsgMaskInfo);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Data"></param>
		/// <param name="DataLength"></param>
		virtual void MessageReceived(const void* Data, int32_t DataLength) = 0;
	};
}