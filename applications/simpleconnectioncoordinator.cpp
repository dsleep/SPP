// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPNatTraversal.h"
#include "json/json.h"
#include "SPPLogging.h"

#include <iomanip>

struct RemoteClient
{
	std::chrono::steady_clock::time_point LastUpdate;
	std::string Name;
	std::string GUID;
	std::string AppName;
	std::string ConnectGUID;
};

using namespace SPP;

using time_point = std::chrono::system_clock::time_point;
std::string serializeTimePoint(const time_point& time, const std::string& format)
{
	std::time_t tt = std::chrono::system_clock::to_time_t(time);
	std::tm tm = *std::gmtime(&tt); //GMT (UTC)
	//std::tm tm = *std::localtime(&tt); //Locale time-zone, usually UTC by default.
	std::stringstream ss;
	ss << std::put_time(&tm, format.c_str());
	return ss.str();
}

LogEntry LOG_COORD("APPCOORD");
int main()
{
	IntializeCore(nullptr);
	GetOSNetwork();

	auto coordSocket = std::make_shared < UDPSocket >(12021);
	using namespace std::chrono_literals;

	std::vector<uint8_t> BufferRead;
	BufferRead.resize(1024);

	std::map<std::string, RemoteClient> Hosts;
	std::map<std::string, RemoteClient> Viewers;

	while (true)
	{
		IPv4_SocketAddress currentAddress;
		auto packetSize = coordSocket->ReceiveFrom(currentAddress, BufferRead.data(), BufferRead.size());

		// we when get an update, we send an update
		if (packetSize > 0)
		{
			auto CurrentTime = std::chrono::steady_clock::now();

			Json::Value root;
			Json::CharReaderBuilder Builder;
			Json::CharReader* reader = Builder.newCharReader();
			std::string Errors;

			bool parsingSuccessful = reader->parse((char*)BufferRead.data(), (char*)(BufferRead.data() + packetSize), &root, &Errors);
			delete reader;
			if (!parsingSuccessful)
			{
				break;
			}

			SPP_LOG(LOG_COORD, LOG_INFO, "RECV DATA: %d", packetSize);

			std::string jsonString((char*)BufferRead.data(), (char*)BufferRead.data() + packetSize);
			SPP_LOG(LOG_COORD, LOG_INFO, "%s", jsonString.c_str());

			Json::Value AppNameValue = root.get("APPNAME", Json::Value::nullSingleton());
			
			// app hosts
			if (AppNameValue.isNull() == false)
			{
				Json::Value NameValue = root.get("NAME", Json::Value::nullSingleton());
				Json::Value SDPValue = root.get("SDP", Json::Value::nullSingleton());
				Json::Value GuidValue = root.get("GUID", Json::Value::nullSingleton());
				
				if (SDPValue.isNull() == false &&
					GuidValue.isNull() == false &&
					NameValue.isNull() == false )
				{
					std::string SDPString = SDPValue.asCString();
					std::string GUIDString = GuidValue.asCString();

					Hosts[SDPString] = RemoteClient{ 
						std::chrono::steady_clock::now(), 
						std::string(NameValue.asCString()),
						GUIDString,
						std::string(AppNameValue.asCString())
						 };

					// do any viewers want tp connect to me?
					std::string ConnectionSDP;
					for (auto& [key, value] : Viewers)
					{
						if (GUIDString == value.ConnectGUID)
						{
							ConnectionSDP = key;
						}
					}

					//MESSAGE BACK TO HOST
					Json::Value JsonMessage;
					JsonMessage["SERVERTIME"] = serializeTimePoint(std::chrono::system_clock::now(), "UTC: %Y-%m-%d %H:%M:%S");
					if (!ConnectionSDP.empty())
					{
						JsonMessage["CONNECTSDP"] = ConnectionSDP;
					}
					Json::StreamWriterBuilder wbuilder;
					std::string StrMessage = Json::writeString(wbuilder, JsonMessage);
					coordSocket->SendTo(currentAddress, StrMessage.c_str(), StrMessage.size());
				}
			}
			// app viewers
			else
			{
				Json::Value NameValue = root.get("NAME", Json::Value::nullSingleton());
				Json::Value SDPValue = root.get("SDP", Json::Value::nullSingleton());
				Json::Value GuidValue = root.get("GUID", Json::Value::nullSingleton());
				Json::Value SendAllValue = root.get("SENDALL", Json::Value::nullSingleton());

				if (SDPValue.isNull() == false &&
					GuidValue.isNull() == false &&
					NameValue.isNull() == false)
				{
					std::string SDPString = SDPValue.asCString();
					std::string GUIDString = GuidValue.asCString();
					std::string ConnectGUID;

					Json::Value ConnectGUIDValue = root.get("CONNECT", Json::Value::nullSingleton());
					if (!ConnectGUIDValue.isNull())
					{
						ConnectGUID = ConnectGUIDValue.asCString();
					}

					Viewers[SDPString] = RemoteClient{
						std::chrono::steady_clock::now(),
						std::string(NameValue.asCString()),
						GUIDString,
						"", //no appname
						ConnectGUID
					};

					// let it know about servertime
					{
						Json::Value JsonMessage;
						JsonMessage["SERVERTIME"] = serializeTimePoint(std::chrono::system_clock::now(), "UTC: %Y-%m-%d %H:%M:%S");
						Json::StreamWriterBuilder wbuilder;
						std::string StrMessage = Json::writeString(wbuilder, JsonMessage);
						coordSocket->SendTo(currentAddress, StrMessage.c_str(), StrMessage.size());
					}

					// send him all the known hosts
					for (auto& [key, value] : Hosts)
					{
						if (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - value.LastUpdate).count() < 5)
						{
							Json::Value JsonMessage;
							JsonMessage["NAME"] = value.Name;
							JsonMessage["APPNAME"] = value.AppName;
							JsonMessage["GUID"] = value.GUID;
							JsonMessage["SDP"] = key;

							Json::StreamWriterBuilder wbuilder;
							std::string StrMessage = Json::writeString(wbuilder, JsonMessage);

							coordSocket->SendTo(currentAddress, StrMessage.c_str(), StrMessage.size());
						}
					}

					// hack to send all clients too
					if (SendAllValue.isNull() == false)
					{
						for (auto& [key, value] : Viewers)
						{
							if (value.GUID != GUIDString &&
								std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - value.LastUpdate).count() < 5)
							{								
								Json::Value JsonMessage;
								JsonMessage["NAME"] = value.Name;
								JsonMessage["APPNAME"] = value.AppName;
								JsonMessage["GUID"] = value.GUID;
								JsonMessage["SDP"] = key;

								std::string ConnectionSDP;
								if (GUIDString == value.ConnectGUID)
								{
									JsonMessage["CONNECTSDP"] = key;
								}

								Json::StreamWriterBuilder wbuilder;
								std::string StrMessage = Json::writeString(wbuilder, JsonMessage);

								coordSocket->SendTo(currentAddress, StrMessage.c_str(), StrMessage.size());
							}
						}
					}
				}
			}
		}

		std::this_thread::sleep_for(1ms);
	}

	return 0;
}