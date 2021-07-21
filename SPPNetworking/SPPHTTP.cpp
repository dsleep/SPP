#include "RedRiverBase.h"
#include "RRHTTP.h"
#include <sstream>

namespace RedRiver
{
	LogEntry LOG_HTTP("HTTP");

	HttpBasicSend::HttpBasicSend(const IPv4_SocketAddress &InAddr, const std::string &hostname, const std::string &url, const std::string &out)
	{		
		std::string request;
		std::stringstream requestBuilder;

		requestBuilder << "POST " << url << " HTTP/1.1" << std::endl;
		requestBuilder << "Host: " << hostname << std::endl;
		requestBuilder << "User-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 5.1; .NET CLR 1.1.4322; .NET CLR 2.0.50727)" << std::endl;
		requestBuilder << "Content-Length: " << out.length() << std::endl;
		requestBuilder << "Content-Type: application/x-www-form-urlencoded" << std::endl;
		requestBuilder << "Accept-Language: en-au" << std::endl;
		requestBuilder << std::endl;
		requestBuilder << out;
		request = requestBuilder.str();

		//connect
		std::unique_ptr<TCPSocket> _socket = std::make_unique< TCPSocket >();
		_socket->Connect(InAddr);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		_socket->Send(request.c_str(), request.length());

		//std::string response;
		//int resp_leng;
		//char buffer[BUFFERSIZE];
		//get response
		//response = "";
		//resp_leng = BUFFERSIZE;
		//while (resp_leng == BUFFERSIZE)
		//{
		//	resp_leng = recv(sock, (char*)&buffer, BUFFERSIZE, 0);
		//	if (resp_leng > 0)
		//		response += string(buffer).substr(0, resp_leng);
		//	//note: download lag is not handled in this code
		//}
		//disconnect
		//closesocket(sock);
		//return  response;
	}
}
	

