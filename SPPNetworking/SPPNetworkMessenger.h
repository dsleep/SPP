// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPNetworking.h"
#include "SPPSerialization.h"
#include <functional>
#include "SPPSockets.h"
#include "SPPNetworkConnection.h"
#include "SPPSTLUtils.h"
#include <set>

#define NETWORK_MESSAGE_TEMPLATE(ID, Name,Func,...) auto MP_##Name = std::make_shared< RedRiver::TMessageProcessor<ID, ##__VA_ARGS__> > ( #Name, Func );

#define CONSTRUCT_NETWORK_MESSAGE(ID, Name,Func,...) NETWORK_MESSAGE_TEMPLATE(ID, Name, Func, ##__VA_ARGS__)

#define CREATE_RPC(funcName,...) \
		using funcName##_MSG = TMessageProcessor<0, ##__VA_ARGS__>; \
		template<typename... Args> \
		void funcName##_CALL(const Args&... InitializerList) \
		{ \
			if (auto strongConn = _weakConnection.lock()) \
			{ \
				funcName##_MSG::SendToNamed(strongConn.get(), #funcName, InitializerList...); \
			} \
		} 

#define CREATE_RPC_AND_DECLARE(funcName,...) \
		CREATE_RPC(funcName, ##__VA_ARGS__); \
		void funcName( __VA_ARGS__ );

#define IMPLEMENT_RPC(funcName,...) \
	{ \
		auto tempMessageFactory = InConnection->GetMessageFactory(); \
		using currentType = typename std::remove_pointer<decltype(this)>::type; \
		std::weak_ptr<currentType> weakUserRef = InConnection->GetUserLink<currentType>(); \
		auto binderFunc = [weakUserRef](auto* netConnection, const auto&... InitializerList) { \
			if(auto strUser = weakUserRef.lock()) { \
				strUser->funcName(InitializerList...); \
			} \
		}; \
		_messageLinks.push_back(std::move(funcName##_MSG::AddToNamed(tempMessageFactory, #funcName, binderFunc))); \
	}

namespace SPP
{
	class SPP_NETWORKING_API MessageSplitTranscoder : public Interface_MessageTranscoder
	{
	protected:
		uint16_t _pendingMessageCount = 0;
		std::vector<uint8_t> _pendingAssembly;

	public:
		// sending back to front
		virtual void Send(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo);
		// something higher up is passing down data
		virtual void Recv(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo);
		virtual void Update(NetworkConnection *InConnection) { };
		virtual std::string ReportString() override;
	};

	class SPP_NETWORKING_API ReliabilityTranscoder : public Interface_MessageTranscoder
	{
		NO_COPY_ALLOWED(ReliabilityTranscoder);

		using HighResClock = std::chrono::high_resolution_clock;

	protected:
		struct MessageIndices
		{
			// random ID's from testing
			uint16_t Reliable = 65500;
			uint16_t Unreliable = 65499;
		} _incomingIndices, _outGoingIndices;

		struct StoredMessage
		{
			uint16_t Index;
			uint8_t SendCount = 0;
			bool bHasSent = false;
			std::vector<uint8_t> Message;
			HighResClock::time_point _lastSendTime;
			StoredMessage(uint16_t InIndex, std::vector<uint8_t>&InMessage)
			{
				Index = InIndex;
				std::swap(Message, InMessage);
				_lastSendTime = HighResClock::now();
			}
			int32_t MemSize() const
			{
				return (int32_t)(sizeof(StoredMessage) + Message.size());
			}
		};

		struct StoredReliableMessage
		{
			uint16_t Index;
			std::vector<uint8_t> Message; 
			
			StoredReliableMessage(uint16_t InIndex, const void* ArrayData, int64_t Length)
			{
				Index = InIndex;
				Message.resize(Length);
				memcpy(Message.data(), ArrayData, Length);
			}
		};

		uint16_t lastSentReliableIdx = 23;
		uint16_t lastRecvReliableIdx = 23;
		int32_t _reliablesAwaitingAck = 0;
		int32_t _currentBufferedAmount = 0;
		int32_t _resendRequests = 0;
		int32_t _resendByTimer = 0;
		std::list< std::unique_ptr< StoredMessage > > _outgoingReliableMessages;
		std::list< std::unique_ptr< StoredReliableMessage > > _incomingReliableMessages;
		HighResClock::time_point _lastRecvAckTime;
		
		uint32_t _sentCount = 0;
		uint32_t _resentCount = 0;
		float _sendHealth = 0.0f;

	public:
		ReliabilityTranscoder()
		{
			_lastRecvAckTime = HighResClock::now();
		}

		// sending back to front
		virtual void Send(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo);
		// something higher up is passing down data
		virtual void Recv(NetworkConnection *InConnection, const void *Data, int32_t DataLength, uint16_t MsgMaskInfo);
		// 
		virtual void Update(NetworkConnection *InConnection);
		//
		virtual int32_t GetBufferedAmount() const override { return _currentBufferedAmount; }
		virtual int32_t GetBufferedMessageCount() const override { return (int32_t) _outgoingReliableMessages.size(); }
		//
		virtual void Report() override;

		virtual std::string ReportString() override;
	};

	class SPP_NETWORKING_API BaseMessageProcessor
	{
	private:
		uint8_t Index;
		std::string Name;

	public:
		virtual void Read(NetworkConnection *InNetworkConnection, MemoryView &InStorageInterface) = 0;
		virtual ~BaseMessageProcessor() {}

		BaseMessageProcessor(const char *InName)
		{
			Name = InName;
		};

		std::string GetName()
		{
			return Name;
		}
	};

	
};


