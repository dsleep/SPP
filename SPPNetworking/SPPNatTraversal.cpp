// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPNatTraversal.h"
#include "SPPLogging.h"
#include "SPPSerialization.h"

#include "juice/juice.h"

extern "C"
{
#include "juice/base64.h"
}

#include <iostream>
#include <thread>
#include <mutex>

namespace SPP
{
	LogEntry LOG_JUICE("juice");
	
	struct UDPJuiceSocket::PlatImpl
	{
		juice_agent_t* agent = nullptr;
		juice_config_t config = { 0 };
		std::atomic_bool bDoneGathering = false;
		bool bHasSetRemoteSDP = false;
		char sdp[JUICE_MAX_SDP_STRING_LEN];
		char sdp64[JUICE_MAX_SDP_STRING_LEN*2];

		std::mutex incDataMutex;
		BinaryBlobSerializer incData;
	};

	static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr)
	{
		auto juiceSocket = static_cast<UDPJuiceSocket*>(user_ptr);

		SPP_LOG(LOG_JUICE, LOG_INFO, "on_state_changed: %s", juice_state_to_string(state));

		if (state == JUICE_STATE_CONNECTED)
		{
			// Agent 1: on connected, send a message
			//const char* message = "NAT traversed peer message!!!!!!";
			//juice_send(gent, message, strlen(message) + 1);
		}
	}

	static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr)
	{
		auto juiceSocket = static_cast<UDPJuiceSocket*>(user_ptr);
		SPP_LOG(LOG_JUICE, LOG_INFO, "on_candidate: %s", sdp);
		// Agent 2: Receive it from agent 1
		//juice_add_remote_candidate(agent2, sdp);
	}

	static void on_gathering_done(juice_agent_t* agent, void* user_ptr) 
	{
		auto juiceSocket = static_cast<UDPJuiceSocket*>(user_ptr);
		SPP_LOG(LOG_JUICE, LOG_INFO, "on_gathering_done");
		juiceSocket->INTERNAL_DoneGathering();
		//juice_set_remote_gathering_done(agent2); // optional
	}

	static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) 
	{
		auto juiceSocket = static_cast<UDPJuiceSocket*>(user_ptr);
		//SPP_LOG(LOG_JUICE, LOG_INFO, "on_recv: %d", size);
		juiceSocket->INTERNAL_DataRecv(data, size);
	}

	UDPJuiceSocket::UDPJuiceSocket() : _impl(new PlatImpl())
	{
		_impl->config.stun_server_host = "stun.stunprotocol.org";
		_impl->config.stun_server_port = 3478;
		_impl->config.turn_servers_count = 0;

		_impl->config.cb_state_changed = on_state_changed;
		_impl->config.cb_candidate = on_candidate;
		_impl->config.cb_gathering_done = on_gathering_done;
		_impl->config.cb_recv = on_recv;
		_impl->config.user_ptr = this;

		_impl->agent = juice_create(&_impl->config);

		juice_get_local_description(_impl->agent, _impl->sdp, JUICE_MAX_SDP_STRING_LEN);
		juice_gather_candidates(_impl->agent);	
	}

	void UDPJuiceSocket::INTERNAL_DataRecv(const char* data, size_t size)
	{
		std::unique_lock<std::mutex> lk(_impl->incDataMutex);
		_impl->incData.Write(data, size);
	}	

	void UDPJuiceSocket::INTERNAL_DoneGathering()
	{
		juice_get_local_description(_impl->agent, _impl->sdp, JUICE_MAX_SDP_STRING_LEN);
		juice_base64_encode(_impl->sdp, std::strlen(_impl->sdp), _impl->sdp64, sizeof(_impl->sdp64));

		_impl->bDoneGathering = true;
		SPP_LOG(LOG_JUICE, LOG_INFO, "\n*** REMOTE SIGNATURE ***\n%s", _impl->sdp);
	}

	bool UDPJuiceSocket::IsReady() const
	{
		return _impl->bDoneGathering;
	}

	const char* UDPJuiceSocket::GetSDP() const
	{
		return _impl->sdp;
	}

	const char* UDPJuiceSocket::GetSDP_BASE64() const
	{
		return _impl->sdp64;
	}

	UDPJuiceSocket::~UDPJuiceSocket()
	{
		juice_destroy(_impl->agent);
	}

	void UDPJuiceSocket::Send(const void* buf, uint16_t BufferSize) 
	{
		//SPP_LOG(LOG_JUICE, LOG_INFO, "juice_send: %d", BufferSize);
		juice_send(_impl->agent, (const char*) buf, BufferSize);
	}

	int32_t UDPJuiceSocket::Receive(void* buf, uint16_t InBufferSize) 
	{
		std::unique_lock<std::mutex> lk(_impl->incDataMutex);
		uint16_t maxWrite = (uint16_t)std::min<uint64_t>(std::numeric_limits<uint16_t>::max(), _impl->incData.Size());
		if(maxWrite)
		{
			// copy that front
			auto& IncomingData = _impl->incData.GetArray();
			memcpy(buf, IncomingData.data(), maxWrite);
			// erase what we wrote
			IncomingData.erase(IncomingData.begin(), IncomingData.begin() + maxWrite);
			// fix size after erase
			_impl->incData.Seek(_impl->incData.Size());
		}
		return maxWrite;
	}

	bool UDPJuiceSocket::IsConnected() 
	{
		auto currentState = juice_get_state(_impl->agent);
		return (currentState == JUICE_STATE_COMPLETED ||
			currentState == JUICE_STATE_CONNECTED);
	}

	bool UDPJuiceSocket::HasRemoteSDP() const
	{
		return _impl->bHasSetRemoteSDP;
	}

	void UDPJuiceSocket::SetRemoteSDP(const char* InDesc)
	{
		if (_impl->bHasSetRemoteSDP == false)
		{
			juice_set_remote_description(_impl->agent, InDesc);
			_impl->bHasSetRemoteSDP = true;
		}
	}

	void UDPJuiceSocket::SetRemoteSDP_BASE64(const char* InDesc)
	{
		if (_impl->bHasSetRemoteSDP == false)
		{
			char remotesdp[JUICE_MAX_SDP_STRING_LEN] = { 0 };
			auto decodePos = juice_base64_decode(InDesc, remotesdp, sizeof(remotesdp));

			if (decodePos > 0)
			{
				juice_set_remote_description(_impl->agent, remotesdp);
				_impl->bHasSetRemoteSDP = true;

				SPP_LOG(LOG_JUICE, LOG_INFO, "SetRemoteSDP_BASE64(decoded): \n%s", remotesdp);
			}
			else
			{
				SPP_LOG(LOG_JUICE, LOG_INFO, "SetRemoteSDP_BASE64(decoded): COULD NOT DECODE");
			}
		}
	}

}
	

