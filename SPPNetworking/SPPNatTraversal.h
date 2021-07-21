// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPNetworking.h"
#include "SPPSockets.h"

namespace SPP
{
	class SPP_NETWORKING_API UDPJuiceSocket : public Interface_PeerConnection
	{
	private:
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

	public:
		UDPJuiceSocket();

		void INTERNAL_DataRecv(const char* data, size_t size);
		void INTERNAL_DoneGathering();

		bool IsReady() const;
		bool IsConnected();
		const char* GetSDP() const;
		const char* GetSDP_BASE64() const;
		bool HasRemoteSDP() const;
		void SetRemoteSDP(const char* InDesc);
		void SetRemoteSDP_BASE64(const char* InDesc);

		virtual void Send(const void* buf, uint16_t BufferSize) override;
		virtual int32_t Receive(void* buf, uint16_t InBufferSize) override;

		virtual ~UDPJuiceSocket();
	};
}
