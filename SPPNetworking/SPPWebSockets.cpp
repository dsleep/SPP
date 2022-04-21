// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPNetworking.h"
#include "SPPWebSockets.h"
#include "SPPLogging.h"

#include "rtc/rtc.hpp"

#include <string.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <list>

namespace SPP
{
	LogEntry LOG_WEBSOCKETS("WebSockets");

	struct WebSocket::PlatImpl
	{
		std::mutex dataM;
		std::vector<uint8_t> data;

		std::shared_ptr<rtc::WebSocket> ws;
		std::shared_ptr<rtc::WebSocketServer> wss;

		std::mutex clienList;
		std::list< std::shared_ptr<rtc::WebSocket> > pendingWSSClients;
	};

	void InitLDCLogging()
	{
		static bool bIsInit = false;

		if (!bIsInit)
		{
			rtcInitLogger(RTC_LOG_VERBOSE, [](rtcLogLevel level, const char* message)
				{
					SPP_LOG(LOG_WEBSOCKETS, LOG_INFO, "libdatachannel: %s", message);
				});
			bIsInit = true;
		}			
	}
	

	WebSocket::WebSocket() : _impl(new PlatImpl())
	{
		InitLDCLogging();
	}
	WebSocket::WebSocket(std::unique_ptr<PlatImpl>&& InImpl) : _impl(std::move(InImpl))
	{
		InitLDCLogging();

		//TODO FIX UP
		auto remoteAddr = InImpl->ws->remoteAddress();
	}

	WebSocket::~WebSocket()
	{					
		if (_impl->ws)
		{
			_impl->ws->close();
			_impl->ws.reset();
		}
		if (_impl->wss)
		{
			_impl->wss->stop();
			_impl->wss.reset();
		}

		_impl->pendingWSSClients.clear();
		_impl->data.clear();
	}

	void WebSocket::Send(const void* buf, uint16_t BufferSize)
	{
		rtc::binary outData;
		outData.resize(BufferSize);
		memcpy(outData.data(), buf, BufferSize);
		_impl->ws->send(outData);
	}

	int32_t WebSocket::Receive(void* buf, uint16_t InBufferSize)
	{
		std::unique_lock<std::mutex> lock(_impl->dataM);
		auto dataSize = std::min((uint16_t)_impl->data.size(), InBufferSize);
		if (dataSize > 0)
		{
			memcpy(buf, _impl->data.data(), dataSize);
			_impl->data.erase(_impl->data.begin(), _impl->data.begin() + dataSize);
			return dataSize;
		}
		return 0;
	}

	void WebSocket::Connect(const std::string& Address)
	{
		_impl->ws = std::make_shared<rtc::WebSocket>();

		_impl->ws->onOpen([]() {
			SPP_LOG(LOG_WEBSOCKETS, LOG_INFO, "WEBSOCKET onOpen");
			});
		_impl->ws->onClosed([]() {
			SPP_LOG(LOG_WEBSOCKETS, LOG_INFO, "WEBSOCKET onClosed");
			});
		_impl->ws->onError([](std::string Error) {
			SPP_LOG(LOG_WEBSOCKETS, LOG_INFO, "WEBSOCKET onError: %s", Error.c_str());
			});

		_impl->ws->onMessage([_impl = _impl.get()](rtc::binary data) {
				std::unique_lock<std::mutex> lock(_impl->dataM);
				
				std::vector<uint8_t>& castData = reinterpret_cast<std::vector<uint8_t>&>(data);
				_impl->data.insert(_impl->data.end(), castData.begin(), castData.end());
			},
			[](std::string data) {
				SPP_LOG(LOG_WEBSOCKETS, LOG_INFO, "WEBSOCKET msg: %s", data.c_str());
			});

		_impl->ws->open(Address.c_str());		
	}

	bool WebSocket::Listen(uint16_t InPort)
	{
		rtc::WebSocketServer::Configuration config{};
		config.port = InPort;

		_impl->wss = std::make_shared<rtc::WebSocketServer>(std::move(config));
		_impl->wss->onClient([_impl = _impl.get()](std::shared_ptr<rtc::WebSocket> InSocket)
			{
				SPP_LOG(LOG_WEBSOCKETS, LOG_INFO, "WEBSOCKET CLIENT CONNECTION");

				std::unique_lock<std::mutex> lock(_impl->clienList);
				_impl->pendingWSSClients.push_back(InSocket);
			}); 


		return true;
	}

	std::shared_ptr<WebSocket> WebSocket::Accept()
	{
		std::unique_lock<std::mutex> lock(_impl->clienList);
		if (!_impl->pendingWSSClients.empty())
		{
			SPP_LOG(LOG_WEBSOCKETS, LOG_INFO, "WebSocket::Accept getting connection");

			auto platData = std::make_unique<PlatImpl>();
			platData->ws = _impl->pendingWSSClients.back();
			_impl->pendingWSSClients.pop_back();

			return std::make_shared<WebSocket>(std::move(platData));
		}
		return nullptr;
	}
}


