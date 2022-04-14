// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPNetworking.h"
#include "SPPString.h"
#include <vector>
#include <string>
#include <list>
#include <thread>
#include <mutex>
#include <functional>

#define IDEAL_NETWORK_MESSAGE_SIZE 1024

namespace SPP
{
	struct SPP_NETWORKING_API IPv4_SocketAddress
	{
		uint16_t Port = 0;

		union UIPWithAddrs
		{
			struct { uint8_t Addr1, Addr2, Addr3, Addr4; };
			uint32_t IPAddr;
		} UIPAddr = { 0 };

		uint64_t Hash() const { return (uint64_t)UIPAddr.IPAddr | ((uint64_t)Port) << 32; };

		IPv4_SocketAddress() { }

		IPv4_SocketAddress(const char* IpAddrAndPort);
		IPv4_SocketAddress(const char *IpAddr, uint16_t InPort);		

		IPv4_SocketAddress(uint8_t InAddr1, uint8_t InAddr2, uint8_t InAddr3, uint8_t InAddr4, uint16_t InPort)
		{
			UIPAddr.Addr1 = InAddr1;
			UIPAddr.Addr2 = InAddr2;
			UIPAddr.Addr3 = InAddr3;
			UIPAddr.Addr4 = InAddr4;
			Port = InPort;
		}	

		bool IsValidRemote() const
		{
			return (UIPAddr.IPAddr != 0 && Port != 0);
		}

		std::string ToString() const
		{
			return std::string_format("%hhu.%hhu.%hhu.%hhu:%hhu", UIPAddr.Addr1, UIPAddr.Addr2, UIPAddr.Addr3, UIPAddr.Addr4, Port);
		}

		bool operator==(const IPv4_SocketAddress &InCompare) const
		{
			return (Port == InCompare.Port && UIPAddr.IPAddr == InCompare.UIPAddr.IPAddr);
		}
	};

	/// <summary>
	/// In most cases these will be treated like a reliable ordered stream
	/// </summary>
	class Interface_PeerConnection
	{
	public:
		virtual bool InterallyReliableOrdered() const { return false; }
		virtual bool IsBroken() const { return false; }
		virtual void Report() { }
		virtual void Send(const void *buf, uint16_t BufferSize) = 0;
		virtual int32_t Receive(void *buf, uint16_t InBufferSize) = 0;
		virtual ~Interface_PeerConnection() {};
	};

	namespace UDPSocketOptions
	{
		enum Value
		{
			None = 0,
			Broadcast = 1
		};
	}

	// async recv thread
	class SPP_NETWORKING_API UDPSocket
	{
	private:
		IPv4_SocketAddress _addr;
		UDPSocketOptions::Value _socketType = { UDPSocketOptions::None };
		bool _IsValid = false;

		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;
				
	public:
		UDPSocket(uint16_t InPort = 0, UDPSocketOptions::Value InSocketType = UDPSocketOptions::None);
		virtual ~UDPSocket();
		bool IsValid() const;
		const IPv4_SocketAddress &GetLocalAddress() const { return _addr; }
		void SendTo(const IPv4_SocketAddress &Address, const void *buf, uint16_t BufferSize);
		int32_t ReceiveFrom(IPv4_SocketAddress &Address, void *buf, uint16_t InBufferSize);
	};

	// a per address UDP sender
	class SPP_NETWORKING_API UDPSendWrapped : public Interface_PeerConnection
	{
	private:
		std::shared_ptr< UDPSocket > _SocketLink;
		IPv4_SocketAddress _AddressLink;

	public:
		UDPSendWrapped(std::shared_ptr< UDPSocket > InSocket, const IPv4_SocketAddress &InAddr) : _SocketLink(InSocket), _AddressLink(InAddr) {}
		virtual ~UDPSendWrapped() { }

		virtual void Send(const void *buf, uint16_t BufferSize)
		{
			_SocketLink->SendTo(_AddressLink, buf, BufferSize);
		}

		//UDP has no unique per address receive
		virtual int32_t Receive(void *buf, uint16_t InBufferSize)
		{
			return 0;
		}		
	};
	
	enum class TCPSocketState
	{
		Closed,
		Listening,
		Connecting,
		Connected,
		Error
	};
	
	// tpc matches socket interface directly
	class SPP_NETWORKING_API TCPSocket : public Interface_PeerConnection
	{
	protected:		
		TCPSocketState _state = TCPSocketState::Closed;
		bool _IsValid = false;
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;
		IPv4_SocketAddress _remoteAddr;
		uint16_t _listenPort = 0;

		// only set false when opened from accept
		bool _bLocalOpen = true;
		bool _bIsBroken = false;

	public:
		TCPSocket();
		TCPSocket(const IPv4_SocketAddress &InRemoteAddr);
		TCPSocket(std::unique_ptr<PlatImpl> &&InImpl, IPv4_SocketAddress InRemoteAddr);
		
		TCPSocketState GetSocketState() const
		{ 
			return _state;
		}
		virtual ~TCPSocket();

		void SetSocketOptions();
		uint16_t GetListenPort() const { return _listenPort; }
		bool IsValid() const;
		IPv4_SocketAddress GetRemoteAddr() const
		{
			return _remoteAddr;
		}
		virtual bool InterallyReliableOrdered() const override { return true; }

		virtual bool IsBroken() const override { return _bIsBroken; }

		virtual void Connect(const IPv4_SocketAddress &Address);
		virtual bool Listen(uint16_t InPort);				
		virtual std::shared_ptr<TCPSocket> Accept();	
		virtual void Send(const void *buf, uint16_t BufferSize) override;
		int32_t Receive(void *buf, uint16_t InBufferSizee)  override;
	};

	class SPP_NETWORKING_API TCPSocketThreaded : public TCPSocket
	{
	protected:
		std::unique_ptr<std::thread> _socketThread;
		std::mutex _recvMutex;
		std::mutex _sendMutex;
		std::vector<uint8_t> _recvBuffer;
		std::vector<uint8_t> _sendBuffer;
		bool _bRunning = false;

	public:
		TCPSocketThreaded();
		TCPSocketThreaded(const IPv4_SocketAddress &InRemoteAddr);
		TCPSocketThreaded(std::unique_ptr<PlatImpl> &&InImpl, IPv4_SocketAddress InRemoteAddr);
		virtual ~TCPSocketThreaded();

		void BeginThreading();
		void EndThreading();
		void THREAD_Run();
		virtual void Report() override;

		virtual void Connect(const IPv4_SocketAddress &Address) override;
		virtual std::shared_ptr<TCPSocket> Accept() override;		
						
		virtual void Send(const void *buf, uint16_t BufferSize) override;
		virtual int32_t Receive(void *buf, uint16_t InBufferSizee) override;
	};

	class Active_UDP_Socket
	{
	protected:
		IPv4_SocketAddress _addr;
		UDPSocketOptions::Value _socketType = { UDPSocketOptions::None };
		bool _IsValid = false;
		bool _bRunning = false;

		std::function<void(const IPv4_SocketAddress&, const void*, uint16_t)> _recvFunc;

		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

		std::unique_ptr<std::thread> _recvThread;
		void _RunThread();

	public:
		Active_UDP_Socket(uint16_t InPort = 0, UDPSocketOptions::Value InSocketType = UDPSocketOptions::None);
		~Active_UDP_Socket();

		bool IsValid();

		void SendTo(const IPv4_SocketAddress& Address, const void* buf, uint16_t BufferSize);
		void StartReceiving(std::function<void(const IPv4_SocketAddress&, const void*, uint16_t)> RecvFunc);
		void StopReceiving();
	};

#if _WIN32
	class SPP_NETWORKING_API BlueToothSocket : public Interface_PeerConnection
	{
	protected:
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

	public:
		BlueToothSocket();
		BlueToothSocket(std::unique_ptr<PlatImpl>&& InImpl);		
		~BlueToothSocket();

		bool Listen();
		bool Connect(char* ConnectionString);
		void SendData(const char* Data, int Size);
		void CloseDown();
		std::shared_ptr< BlueToothSocket > Accept();
					
		virtual bool IsBroken() const override; 
		virtual void Send(const void* buf, uint16_t BufferSize) override;
		virtual int32_t Receive(void* buf, uint16_t InBufferSize) override;
	};
#endif

	class SPP_NETWORKING_API OSNetwork
	{
		friend SPP_NETWORKING_API OSNetwork& GetOSNetwork();

	private:
		OSNetwork();
		~OSNetwork();

	public:
		std::string HostName;
		std::vector< IPv4_SocketAddress> InterfaceAddresses;
	};


	SPP_NETWORKING_API OSNetwork& GetOSNetwork();
	SPP_NETWORKING_API bool IsOpenUDPPort(uint16_t InPort);	
}

namespace std
{
	template<> struct less<SPP::IPv4_SocketAddress>
	{
		bool operator() (const SPP::IPv4_SocketAddress& lhs, const SPP::IPv4_SocketAddress& rhs) const
		{
			return lhs.Hash() < rhs.Hash();
		}
	};
}
