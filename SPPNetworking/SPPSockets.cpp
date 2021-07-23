// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPNetworking.h"
#include "SPPSockets.h"
#include "SPPLogging.h"

#include <iostream>
#include <thread>
#include <mutex>

#if _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif

	#ifndef NOMINMAX
	#define NOMINMAX
	#endif

	#include "Windows.h"

	#include "winsock2.h"
	#include "ws2def.h"
	#include <ws2tcpip.h>
	using socklen_t = int32_t;
	using sock_opt = char;
	#pragma comment(lib, "Ws2_32.lib")

	#define SOCKET_VALID(x) (x != INVALID_SOCKET)
	#define HAS_SOCKET_ERROR(x) (x == SOCKET_ERROR)
	#define HAS_CRITICAL_SOCKET_ERROR(x) (x != WSAEWOULDBLOCK && x != WSAEALREADY && x != WSAENOTCONN)
	
#else
	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/udp.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include <ifaddrs.h>
	#include <errno.h> 

	using SOCKET = int;
	using sock_opt = int;
	#define INVALID_SOCKET 0

	#define HAS_SOCKET_ERROR(x) (x < 0)
	#define HAS_CRITICAL_SOCKET_ERROR(x) (x != EAGAIN && x != EWOULDBLOCK && x != ENOTCONN)
	#define SOCKET_VALID(x) (x >= 0)
#endif

#define OS_SOCK_BUFFER_SIZES (10 * 1024 * 1024)
	
#include <cmath>

namespace SPP
{
	LogEntry LOG_SOCKETS("Sockets");
	LogEntry LOG_UDP("UDPSocket");
		
	IPv4_SocketAddress ToIPv4_SocketAddress(const sockaddr_in &InAddr)
	{
		IPv4_SocketAddress Out;
		Out.Port = ntohs(InAddr.sin_port);
		Out.UIPAddr.IPAddr = (InAddr.sin_addr.s_addr);
		return Out;
	}

	sockaddr_in ToSockAddr_In(const IPv4_SocketAddress &InAddr)
	{
		sockaddr_in address = { 0 };
		address.sin_family = AF_INET;
		address.sin_port = htons(InAddr.Port);
		address.sin_addr.s_addr = (InAddr.UIPAddr.IPAddr);
		return address;
	}

#if _WIN32

	OSNetwork& GetOSNetwork()
	{
		static OSNetwork sO;
		return sO;
	}

	void StartOSNetworkingSystems(OSNetwork *InMaster)
	{
		WSADATA data;
		WSAStartup(MAKEWORD(2, 2), &data);

		char ac[80];
		if (gethostname(ac, sizeof(ac)) != SOCKET_ERROR) 
		{
			SPP_LOG(LOG_SOCKETS, LOG_INFO, "Host Name: %s", ac);
			InMaster->HostName = ac;
		}

		SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
		if (sd == SOCKET_ERROR) 
		{
			SPP_LOG(LOG_SOCKETS, LOG_INFO, "Failed to get a socket. Error %d ", WSAGetLastError());
			return;
		}

		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;
		if (WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
			sizeof(InterfaceList), &nBytesReturned, 0, 0) == SOCKET_ERROR) 
		{
			SPP_LOG(LOG_SOCKETS, LOG_INFO, "Failed calling WSAIoctl: error %d", WSAGetLastError());
			return;
		}

		int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);
		SPP_LOG(LOG_SOCKETS, LOG_INFO, "There are %d interfaces:", nNumInterfaces);
		for (int i = 0; i < nNumInterfaces; ++i)
		{
			std::cout << std::endl;

			sockaddr_in *pAddress;
			pAddress = (sockaddr_in *) & (InterfaceList[i].iiAddress);
			SPP_LOG(LOG_SOCKETS, LOG_INFO, "(%s)", inet_ntoa(pAddress->sin_addr));
			pAddress = (sockaddr_in *) & (InterfaceList[i].iiBroadcastAddress);
			SPP_LOG(LOG_SOCKETS, LOG_INFO, " - has bcast %s", inet_ntoa(pAddress->sin_addr));
			pAddress = (sockaddr_in *) & (InterfaceList[i].iiNetmask);
			SPP_LOG(LOG_SOCKETS, LOG_INFO, " - netmask %s", inet_ntoa(pAddress->sin_addr));

			
			u_long nFlags = InterfaceList[i].iiFlags;

			SPP_LOG(LOG_SOCKETS, LOG_INFO, " Iface is %s", (nFlags & IFF_UP) ? "up" : "down");

			if (nFlags & IFF_POINTTOPOINT)
			{
				SPP_LOG(LOG_SOCKETS, LOG_INFO, " - point-to-point");
			}

			if (nFlags & IFF_LOOPBACK)
			{
				SPP_LOG(LOG_SOCKETS, LOG_INFO, " - loopback interface");
			}
			else
			{
				auto ipaddr = (sockaddr_in *) & (InterfaceList[i].iiAddress);
				InMaster->InterfaceAddresses.push_back(ToIPv4_SocketAddress(*ipaddr));
			}

			if (nFlags & IFF_BROADCAST)
			{
				SPP_LOG(LOG_SOCKETS, LOG_INFO, " - broadcast supported ");
			}
			if (nFlags & IFF_MULTICAST)
			{
				SPP_LOG(LOG_SOCKETS, LOG_INFO, " - multicast supported ");
			}
		}

		closesocket(sd);
	}

	void ShutdownOSNetworking()
	{
		WSACleanup();
	}

	OSNetwork::OSNetwork()
	{
		StartOSNetworkingSystems(this);
	}

	OSNetwork::~OSNetwork()
	{
		WSACleanup();
	}

#else
	void StartOSNetworkingSystems()
	{		
		char ac[80];
		if (gethostname(ac, sizeof(ac)) == 0) 
		{
			SPP_LOG(LOG_SOCKETS, LOG_INFO, "Host Name: %s", ac);
			HostName = ac;
		}
		
		struct ifaddrs * ifAddrStruct = NULL;
		struct ifaddrs * ifa = NULL;
		void * tmpAddrPtr = NULL;

		getifaddrs(&ifAddrStruct);

		for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
		{
			if (!ifa->ifa_addr) {
				continue;
			}
			if (ifa->ifa_addr->sa_family == AF_INET) 
			{
				 // check it is IP4
			    // is a valid IP4 Address
				sockaddr_in *pAddress = (struct sockaddr_in *)ifa->ifa_addr;
				
			    tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
				char addressBuffer[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
				
				std::string IPName = ifa->ifa_name;
				
				auto findIdx = IPName.find_first_of("eth");
				
				if (findIdx == 0)
				{
					auto IPAddr = ToIPv4_SocketAddress(*pAddress);
					SPP_LOG(LOG_SOCKETS, LOG_INFO, "LOCAL %s IP Address %s", ifa->ifa_name, IPAddr.ToString().c_str()); 						
					InterfaceAddresses.push_back(IPAddr);
				}				
			}
		}
		if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);
		
	}

	void ShutdownOSNetworking()
	{
	}
#endif

	IPv4_SocketAddress::IPv4_SocketAddress(const char* IpAddrAndPort)
	{
		sscanf(IpAddrAndPort, "%hhu.%hhu.%hhu.%hhu:%hu", &UIPAddr.Addr1, &UIPAddr.Addr2, &UIPAddr.Addr3, &UIPAddr.Addr4, &Port);
	}

	IPv4_SocketAddress::IPv4_SocketAddress(const char *IpAddr, uint16_t InPort)
	{
		inet_pton(
			AF_INET,
			IpAddr,
			&UIPAddr.IPAddr
		);
		Port = InPort;
	}
	
	
	struct UDPSocket::PlatImpl
	{
		SOCKET Socket = INVALID_SOCKET;

	};

	void UDPSocket::SendTo(const IPv4_SocketAddress &Address, const void *buf, uint16_t BufferSize)
	{
		sockaddr_in Platform_Addr = ToSockAddr_In(Address);
		int32_t socketValue = sendto(_impl->Socket, (const char*)buf, (int)BufferSize, 0, (sockaddr *)&Platform_Addr, sizeof(sockaddr_in));

		if (HAS_SOCKET_ERROR(socketValue))
		{
#if _WIN32
			socketValue = WSAGetLastError();
#endif
			SPP_LOG(LOG_UDP, LOG_WARNING, "UDPSocket::SendTo error %d", socketValue);
			return;
		}
		
		SE_ASSERT(socketValue == BufferSize);
	}

	int32_t UDPSocket::ReceiveFrom(IPv4_SocketAddress &Address, void *buf, uint16_t InBufferSize)
	{
		struct sockaddr_in Platform_Addr = { 0 };
		Platform_Addr.sin_family = AF_INET;
		Platform_Addr.sin_addr.s_addr = INADDR_ANY;

		socklen_t SockLength = sizeof(sockaddr_in);
		int32_t DataRecv = recvfrom(_impl->Socket, (char*)buf, InBufferSize, 0, (sockaddr *)&Platform_Addr, &SockLength);
		Address = ToIPv4_SocketAddress(Platform_Addr);


		if (HAS_SOCKET_ERROR(DataRecv))
		{
#if _WIN32
			int errorCode = WSAGetLastError();
			if (errorCode != WSAEWOULDBLOCK)
			{
				SPP_LOG(LOG_UDP, LOG_VERBOSE, "UDPSocket::LowLevelReceiveFrom error %d", errorCode);
			}
#endif
		}

		return DataRecv;
	}
		
	UDPSocket::UDPSocket(uint16_t InPort, UDPSocketOptions::Value InSocketType) : _impl(new PlatImpl()), _socketType(InSocketType)
	{
		SPP_LOG(LOG_UDP, LOG_INFO, "Create UDPSocket Port %d Type %d", InPort, InSocketType);

		// Creating socket file descriptor 
		if ((_impl->Socket = socket(AF_INET, SOCK_DGRAM, 0)) == 0)
		{
			SPP_LOG(LOG_UDP, LOG_WARNING, " - failed socket");
			return; //return if socket cannot be created
		}

		if (_socketType == UDPSocketOptions::Broadcast)
		{
			sock_opt opt = 1;
			if (setsockopt(_impl->Socket,
				SOL_SOCKET,
				SO_BROADCAST,
				&opt,
				sizeof(opt)))
			{
				return; //return if socket cannot be set to Broadcast
			}
		}		

		sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(InPort);

		if (InPort)
		{			
			// Forcefully attaching socket to the port  
			if (bind(_impl->Socket, (struct sockaddr*)&address, sizeof(address)) < 0)
			{
				SPP_LOG(LOG_UDP, LOG_WARNING, " - failed binding");
				return;
			}
		}

		int32_t bufsize = OS_SOCK_BUFFER_SIZES;
		socklen_t len = sizeof(bufsize);
		setsockopt(_impl->Socket, SOL_SOCKET, SO_RCVBUF, (const char *)&bufsize, sizeof(int));		
		getsockopt(_impl->Socket, SOL_SOCKET, SO_RCVBUF, (char*) &bufsize, &len);
		//SE_ASSERT(bufsize >= OS_SOCK_BUFFER_SIZES);
		setsockopt(_impl->Socket, SOL_SOCKET, SO_SNDBUF, (const char *)&bufsize, sizeof(int));
		getsockopt(_impl->Socket, SOL_SOCKET, SO_SNDBUF, (char*)&bufsize, &len);
		//SE_ASSERT(bufsize >= OS_SOCK_BUFFER_SIZES);
		// where socketfd is the socket you want to make non-blocking
#if _WIN32
		u_long iMode = 1;
		ioctlsocket(_impl->Socket, FIONBIO, &iMode);
#else
		fcntl(_impl->Socket, F_SETFL, fcntl(_impl->Socket, F_GETFL, 0) | O_NONBLOCK);
#endif

		len = sizeof(address);
		if (getsockname(_impl->Socket, (struct sockaddr*)&address, &len) != -1)
		{
			_addr = ToIPv4_SocketAddress(address);
		}

		SPP_LOG(LOG_UDP, LOG_WARNING, " - socket created!!!");

		_IsValid = true;
	}

	bool UDPSocket::IsValid() const
	{
		return _IsValid; //other checks?
	}

	UDPSocket::~UDPSocket()
	{
#if _WIN32
		closesocket(_impl->Socket);
#else
		close(_impl->Socket);
#endif
	}

	LogEntry LOG_TCP("TCPSocket");

	struct TCPSocket::PlatImpl
	{
		SOCKET Socket = INVALID_SOCKET;

		PlatImpl() { }
		PlatImpl(SOCKET InSocket) : Socket(InSocket) { }
	};

	TCPSocket::TCPSocket() : _impl(new PlatImpl())
	{
		// Creating socket file descriptor 
		if ((_impl->Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == 0)
		{
			return;
		}
					   
		SetSocketOptions();
		
		_IsValid = true;
	}

	TCPSocket::TCPSocket(const IPv4_SocketAddress &InRemoteAddr) : TCPSocket()
	{
		_remoteAddr = InRemoteAddr;
	}

	TCPSocket::TCPSocket(std::unique_ptr<PlatImpl> &&InImpl, IPv4_SocketAddress InRemoteAddr) : _impl(std::move(InImpl)), _remoteAddr(InRemoteAddr)
	{
	}

	void TCPSocket::Connect(const IPv4_SocketAddress &Address)
	{
		_state = TCPSocketState::Connecting;

		sockaddr_in Platform_Addr = ToSockAddr_In(Address);

		int result = connect(_impl->Socket, (sockaddr *)&Platform_Addr, sizeof(sockaddr_in));
		
		if (HAS_SOCKET_ERROR(result))
		{
#if _WIN32
			int errorCode = WSAGetLastError();
			switch (errorCode)
			{
			case WSAEISCONN:    
				SPP_LOG(LOG_TCP, LOG_WARNING, "already connected");				
			case WSAEWOULDBLOCK:
			case WSAEALREADY:
				break;
			default:
				SPP_LOG(LOG_TCP, LOG_WARNING, "client connect() error %d\n", errorCode);
				return;
			}
#else
			SPP_LOG(LOG_TCP, LOG_WARNING, "client connect() error %d\n", result);
			return;
#endif
		}
	}
	TCPSocket::~TCPSocket()
	{
#if _WIN32
		closesocket(_impl->Socket);
#else
		close(_impl->Socket);
#endif
	}
	bool TCPSocket::IsValid() const
	{
		return _IsValid;
	}

	bool TCPSocket::Listen(uint16_t InPort)
	{
		_listenPort = InPort;
			 
		sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(_listenPort);

		// start listening on the server
		int result = bind(_impl->Socket, (struct sockaddr *)&address, sizeof(sockaddr_in));

		if (HAS_SOCKET_ERROR(result))
		{
#if _WIN32
			result = WSAGetLastError();
#endif
			SPP_LOG(LOG_TCP, LOG_WARNING, "bind() error %d", result);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			return false;
		}
		result = listen(_impl->Socket, /* size of connection queue */10);
		if (HAS_SOCKET_ERROR(result))
		{
#if _WIN32
			result = WSAGetLastError();
#endif
			SPP_LOG(LOG_TCP, LOG_WARNING, "listen() error %d", result);
			
			return false;
		}
				
		{
			SPP_LOG(LOG_TCP, LOG_WARNING, "is listening on port %u", InPort);
			
		}			

		SPP_LOG(LOG_TCP, LOG_WARNING, "created listen socket %u", InPort);

		_state = TCPSocketState::Listening;
		return true;
	}
	   
	void TCPSocket::Send(const void *buf, uint16_t BufferSize)
	{
		if (_bIsBroken) return;

		int result = send(_impl->Socket, (const char*)buf, (int) BufferSize, 0);

		if (HAS_SOCKET_ERROR(result))		
		{
#if _WIN32
			result = WSAGetLastError();
#else
			result = errno;
#endif
			if (HAS_CRITICAL_SOCKET_ERROR(result))
			{
				SPP_LOG(LOG_TCP, LOG_WARNING, "TCPSocket::Send error %d", result);
				_bIsBroken = true;
			}				
			return;
		}		
	}

	void TCPSocket::SetSocketOptions()
	{
		// where socketfd is the socket you want to make non-blocking
#if _WIN32
		u_long iMode = 1;
		ioctlsocket(_impl->Socket, FIONBIO, &iMode);
#else
		fcntl(_impl->Socket, F_SETFL, fcntl(_impl->Socket, F_GETFL, 0) | O_NONBLOCK);
#endif		

		int32_t bufsize = OS_SOCK_BUFFER_SIZES;
		socklen_t len = sizeof(bufsize);
		setsockopt(_impl->Socket, SOL_SOCKET, SO_RCVBUF, (const char *)&bufsize, sizeof(int));
		getsockopt(_impl->Socket, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, &len);
		//RR_ASSERT(bufsize >= OS_SOCK_BUFFER_SIZES);
		setsockopt(_impl->Socket, SOL_SOCKET, SO_SNDBUF, (const char *)&bufsize, sizeof(int));
		getsockopt(_impl->Socket, SOL_SOCKET, SO_SNDBUF, (char*)&bufsize, &len);
		//RR_ASSERT(bufsize >= OS_SOCK_BUFFER_SIZES);
	}

	int32_t TCPSocket::Receive(void *buf, uint16_t InBufferSize)
	{
		if (_bIsBroken) return -1;
		//ioctlsocket(mySocket, FIONREAD, &howMuchInBuffer);
		// 4 bytes at a time out of the socket, stored in userInput
		int DataRecv = recv(_impl->Socket, (char*)buf, InBufferSize, 0);

		if (HAS_SOCKET_ERROR(DataRecv))
		{
#if _WIN32
			DataRecv = WSAGetLastError();
#else
			DataRecv = errno;
#endif
			if (HAS_CRITICAL_SOCKET_ERROR(DataRecv))
			{
				_bIsBroken = true;
				SPP_LOG(LOG_TCP, LOG_WARNING, "TCPSocket::Receive error %d", DataRecv);
			}

			//SPP_LOG(LOG_TCP, LOG_WARNING, "TCPSocket::Receive error %d", DataRecv);	
			return -1;
		}
		
		return DataRecv;
	}

	std::shared_ptr<TCPSocket> TCPSocket::Accept()
	{
		struct sockaddr_in Platform_Addr = { 0 };
		Platform_Addr.sin_family = AF_INET;
		Platform_Addr.sin_addr.s_addr = INADDR_ANY;
		socklen_t SockLength = sizeof(sockaddr_in);

		auto OSSocket = accept(_impl->Socket, (struct sockaddr*)&Platform_Addr, &SockLength);
		if (SOCKET_VALID(OSSocket) == false)
		{
			return nullptr;
		}		

		IPv4_SocketAddress Address = ToIPv4_SocketAddress(Platform_Addr);

		SPP_LOG(LOG_TCP, LOG_WARNING, "TCPSocket::Accept Address %s", Address.ToString().c_str());
		
		std::shared_ptr< TCPSocket > NewSocket = std::make_shared< TCPSocket >(std::make_unique<PlatImpl>(OSSocket), Address);
		NewSocket->_state = TCPSocketState::Connected;
		NewSocket->_bLocalOpen = false;

		NewSocket->SetSocketOptions();

		return NewSocket;
	}

	////////////////////////////
	TCPSocketThreaded::TCPSocketThreaded() : TCPSocket()
	{
	}

	TCPSocketThreaded::TCPSocketThreaded(const IPv4_SocketAddress &InRemoteAddr) : TCPSocket(InRemoteAddr)
	{
	}

	TCPSocketThreaded::TCPSocketThreaded(std::unique_ptr<PlatImpl> &&InImpl, IPv4_SocketAddress InRemoteAddr) : TCPSocket(std::move(InImpl), InRemoteAddr)
	{
	}

	void TCPSocketThreaded::Connect(const IPv4_SocketAddress &Address)
	{
		TCPSocket::Connect(Address);
		BeginThreading();
	}

	std::shared_ptr<TCPSocket> TCPSocketThreaded::Accept()
	{
		struct sockaddr_in Platform_Addr = { 0 };
		Platform_Addr.sin_family = AF_INET;
		Platform_Addr.sin_addr.s_addr = INADDR_ANY;
		socklen_t SockLength = sizeof(sockaddr_in);

		auto OSSocket = accept(_impl->Socket, (struct sockaddr*)&Platform_Addr, &SockLength);
		if (SOCKET_VALID(OSSocket) == false)
		{
			return nullptr;
		}

		IPv4_SocketAddress Address = ToIPv4_SocketAddress(Platform_Addr);

		SPP_LOG(LOG_TCP, LOG_WARNING, "TCPSocket::Accept Address %s", Address.ToString().c_str());

		std::shared_ptr< TCPSocketThreaded > NewSocket = std::make_shared< TCPSocketThreaded >(std::make_unique<PlatImpl>(OSSocket), Address);
		NewSocket->_state = TCPSocketState::Connected;
		NewSocket->_bLocalOpen = false;

		NewSocket->SetSocketOptions();
		NewSocket->BeginThreading();

		return NewSocket;
	}

	TCPSocketThreaded::~TCPSocketThreaded()
	{
		EndThreading();	
	}
	void TCPSocketThreaded::BeginThreading()
	{
		if (_bRunning == false)
		{
			_bRunning = true;
			SE_ASSERT(!(_socketThread));
			_socketThread = std::make_unique<std::thread>(&TCPSocketThreaded::THREAD_Run, this);
		}
	}
	void TCPSocketThreaded::EndThreading()
	{
		if (_bRunning)
		{
			_bRunning = false;
		}
		if (_socketThread)
		{
			SE_ASSERT(_socketThread->joinable());
			_socketThread->join();
			_socketThread = nullptr;		
		}
	}

	void TCPSocketThreaded::Report()
	{
		std::unique_lock<std::mutex> lckrec(_recvMutex);
		std::unique_lock<std::mutex> lcksend(_sendMutex);
		SPP_LOG(LOG_TCP, LOG_VERBOSE, " - TCPSocketThreaded recv buffer %d send buffer %d", _recvBuffer.size(), _sendBuffer.size());
	}

	void TCPSocketThreaded::THREAD_Run()
	{
		std::vector<uint8_t> localRecv;
		localRecv.resize(10 * 1024 * 1024);

		while (_bRunning)
		{
			//DATA RECV
			{
				int result = recv(_impl->Socket, (char*)localRecv.data(), localRecv.size(), 0);
						
				if (HAS_SOCKET_ERROR(result))
				{
#if _WIN32
					result = WSAGetLastError();
#else
					result = errno;
#endif
					if (HAS_CRITICAL_SOCKET_ERROR(result))
					{
						_bRunning = false;
						_bIsBroken = true;
						SPP_LOG(LOG_TCP, LOG_WARNING, "recv error %d", result);
						break;
					}
				}
				else if (result > 0)
				{
					std::unique_lock<std::mutex> lck(_recvMutex);
					_recvBuffer.insert(_recvBuffer.end(), localRecv.begin(), localRecv.begin() + result);					
				}	
				else if (result == 0)
				{
					_bRunning = false;
					_bIsBroken = true;
					SPP_LOG(LOG_TCP, LOG_WARNING, "recv socket close result");
					break;
				}
			}
			
			//DATA SEND
			{
				std::unique_lock<std::mutex> lck(_sendMutex);
				auto sendAmount = std::min<int32_t>(50000, _sendBuffer.size());
				if (sendAmount > 0)
				{
					int result = send(_impl->Socket, (const char*)_sendBuffer.data(), sendAmount, 0);
								
					if (HAS_SOCKET_ERROR(result))
					{
#if _WIN32
						result = WSAGetLastError();
#else
						result = errno;
#endif
						if (HAS_CRITICAL_SOCKET_ERROR(result))
						{
							_bRunning = false;
							_bIsBroken = true;
							SPP_LOG(LOG_TCP, LOG_WARNING, "recv error %d", result);
							break;
						}
					}
					else
					{
						if(result != sendAmount)						
						{
							SPP_LOG(LOG_TCP, LOG_WARNING, "*******send amount differed %d versus %d", result, sendAmount);
							sendAmount = result;
						}

						if(sendAmount > 0)
						{
							_sendBuffer.erase(_sendBuffer.begin(), _sendBuffer.begin() + sendAmount);
						}
					}
				}
			}
			
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	int32_t TCPSocketThreaded::Receive(void *buf, uint16_t InBufferSize)
	{
		if (_bIsBroken) return -1;

		std::unique_lock<std::mutex> lck(_recvMutex);
		if (_recvBuffer.size() > 0)
		{
			auto copyAmount = std::min<int32_t>(InBufferSize, _recvBuffer.size());
			memcpy(buf, _recvBuffer.data(), copyAmount);
			_recvBuffer.erase(_recvBuffer.begin(), _recvBuffer.begin() + copyAmount);
			return copyAmount;
		}
		return -1;
	}

	void TCPSocketThreaded::Send(const void *buf, uint16_t BufferSize)
	{	
		if (_bIsBroken) return;

		std::unique_lock<std::mutex> lck(_sendMutex);
		_sendBuffer.insert(_sendBuffer.end(), (uint8_t*)buf, (uint8_t*)buf + BufferSize);
	}
}
	

