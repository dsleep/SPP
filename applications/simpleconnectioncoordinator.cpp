// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPNatTraversal.h"
#include "json/json.h"
#include "SPPLogging.h"
#include "SPPTiming.h"
#include "SPPJsonUtils.h"

#include <filesystem>
#include <iomanip>

using namespace SPP;
using namespace std::chrono_literals;

IPv4_SocketAddress RemoteCoordAddres;
LogEntry LOG_COORD("APPCOORD");
int main()
{
	IntializeCore(nullptr);

	{
		Json::Value JsonConfig;
		SE_ASSERT(FileToJson("config.txt", JsonConfig));
		Json::Value COORDINATOR_IP = JsonConfig.get("COORDINATOR_IP", Json::Value::nullSingleton());
		SE_ASSERT(!COORDINATOR_IP.isNull());
		RemoteCoordAddres = IPv4_SocketAddress(COORDINATOR_IP.asCString());
	}

	GetOSNetwork();

	std::filesystem::remove("coordDB.db");

	std::unique_ptr<UDP_SQL_Coordinator> coordinator( new UDP_SQL_Coordinator(RemoteCoordAddres.Port,
			{ 
				{ "GUID", ESQLFieldType::Text},
				{ "SDP", ESQLFieldType::Text},
				{ "NAME", ESQLFieldType::Text},
				{ "APPNAME", ESQLFieldType::Text},
				{ "GUIDCONNECTTO", ESQLFieldType::Text},
				{ "LASTUPDATETIME", ESQLFieldType::Direct}
		}));


	SimplePolledRepeatingTimer< std::chrono::seconds > clearOldOnes;

	clearOldOnes.Initialize([localCoord = coordinator.get()]()
	{
		std::string sql = "DELETE FROM clients WHERE LASTUPDATETIME <= datetime('now','-10 second')";
		localCoord->SQLRequest(sql.c_str());
	}, 10);

	while (true)
	{
		coordinator->Update();
		clearOldOnes.Poll();

		std::this_thread::sleep_for(1ms);
	}

	return 0;
}