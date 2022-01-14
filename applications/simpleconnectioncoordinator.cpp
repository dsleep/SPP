// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPNatTraversal.h"
#include "json/json.h"
#include "SPPLogging.h"
#include "SPPTiming.h"
#include "SPPJsonUtils.h"
#include "SPPNetworkConnection.h"
#include "SPPFileSystem.h"
#include "SPPHandledTimers.h"

#include <iomanip>

SPP_OVERLOAD_ALLOCATORS

using namespace SPP;
using namespace std::chrono_literals;

IPv4_SocketAddress RemoteCoordAddres;
std::string Password;

LogEntry LOG_COORD("APPCOORD");
int main()
{
	IntializeCore(nullptr);

	{
		Json::Value JsonConfig;
		SE_ASSERT(FileToJson("config.txt", JsonConfig));
		Json::Value COORDINATOR_IP = JsonConfig.get("COORDINATOR_IP", Json::Value::nullSingleton());
		Json::Value COORD_PASS = JsonConfig.get("COORDINATOR_PASSWORD", Json::Value::nullSingleton());
		
		SE_ASSERT(!COORDINATOR_IP.isNull());
		SE_ASSERT(!COORD_PASS.isNull());

		RemoteCoordAddres = IPv4_SocketAddress(COORDINATOR_IP.asCString());
		Password = COORD_PASS.asCString();
	}

	GetOSNetwork();

	stdfs::remove("coordDB.db");

	std::unique_ptr<UDP_SQL_Coordinator> coordinator( new UDP_SQL_Coordinator(RemoteCoordAddres.Port,
			{ 
				{ "GUID", ESQLFieldType::Text},
				{ "SDP", ESQLFieldType::Text},
				{ "NAME", ESQLFieldType::Text},
				{ "APPNAME", ESQLFieldType::Text},
				{ "APPCL", ESQLFieldType::Text},
				{ "GUIDCONNECTTO", ESQLFieldType::Text},
				{ "LASTUPDATETIME", ESQLFieldType::Direct}
		}));

	coordinator->SetPassword(Password);

	TimerController mainController(16ms);

	// COORDINATOR UPDATES
	mainController.AddTimer(500ms, true, [&]()
		{
			coordinator->Update();
		});

	// Clear out oldies
	mainController.AddTimer(2.5s, true, [&]()
		{
			std::string sql = "DELETE FROM clients WHERE LASTUPDATETIME <= datetime('now','-5 second')";
			coordinator->SQLRequest(sql.c_str());
		});

	mainController.Run();

	return 0;
}