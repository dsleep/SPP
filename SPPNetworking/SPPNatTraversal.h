// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPNetworking.h"
#include "SPPSockets.h"
#include "SPPDatabase.h"

namespace SPP
{	
	class SPP_NETWORKING_API UDP_SQL_Coordinator
	{
	private:
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

	public:
		UDP_SQL_Coordinator(const IPv4_SocketAddress& InRemoteAddr, bool bInReportToServer = true);
		UDP_SQL_Coordinator(uint16_t InPort, const std::vector<TableField>& InFields);
		~UDP_SQL_Coordinator();

		void SetPassword(const std::string& InPWD);

		void SetSQLRequestCallback(std::function<void(const std::string&)> InReponseFunc);
		void SQLRequest(const std::string& InSQL);
		void SetKeyPair(const std::string& Key, const std::string& Value);
		void GetLocalKeyValue(const std::string& Key, std::string& Value);

		bool IsConnected() const;
		void Update();
	};

	class SPP_NETWORKING_API UDPJuiceSocket : public Interface_PeerConnection
	{
	private:
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;
		
	public:
		UDPJuiceSocket(const IPv4_SocketAddress& InRemoteAddr);
		UDPJuiceSocket(const char *Addr, uint16_t InPort);

		void INTERNAL_DataRecv(const char* data, size_t size);
		void INTERNAL_DoneGathering();

		bool IsReady() const;
		bool IsConnected();
		
		bool HasProblem();

		const char* GetSDP() const;
		const char* GetSDP_BASE64() const;
		bool HasRemoteSDP() const;
		void SetRemoteSDP(const char* InDesc);
		void SetRemoteSDP_BASE64(const char* InDesc);

		virtual void Send(const void* buf, uint16_t BufferSize) override;
		virtual int32_t Receive(void* buf, uint16_t InBufferSize) override;

		virtual ~UDPJuiceSocket();
	};

	//NOT WORKING, STARTED WITH THE STUN FILES...
	class SPP_NETWORKING_API UDPStunServer 
	{
	private:
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

	public:
		UDPStunServer(uint16_t InPort);
		virtual ~UDPStunServer();

		void Update();
	};
}
