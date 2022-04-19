// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPNetworking.h"
#include "SPPSockets.h"

namespace SPP
{
	class SPP_NETWORKING_API WebSocket : public Interface_PeerConnection
	{
	private:
		IPv4_SocketAddress _addr;
		bool _IsValid = false;

		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

		// only set false when opened from accept
		bool _bLocalOpen = true;
		bool _bIsBroken = false;

	public:
		WebSocket();
		WebSocket(std::unique_ptr<PlatImpl>&& InImpl);

		virtual ~WebSocket();
		bool IsValid() const;
		const IPv4_SocketAddress& GetLocalAddress() const { return _addr; }


		virtual void Connect(const std::string& Address);
		virtual bool Listen(uint16_t InPort);
		virtual std::shared_ptr<WebSocket> Accept();

		virtual bool InterallyReliableOrdered() const override { return true; }
		virtual bool IsBroken() const override { return _bIsBroken; }
		virtual void Report() override {}
		virtual void Send(const void* buf, uint16_t BufferSize) override;
		virtual int32_t Receive(void* buf, uint16_t InBufferSize) override;
	};

}