// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

struct LANConfiguration
{
	uint16_t port = 12030;

	bool operator == (const LANConfiguration& cmp) const
	{
		return port == cmp.port;
	}
};

struct CoordinatorConfiguration
{
	std::string addr = "127.0.0.1:12021";
	std::string pwd = "test";

	bool operator == (const CoordinatorConfiguration& cmp) const
	{
		return addr == cmp.addr && pwd == cmp.pwd;
	}
};

struct STUNConfiguration
{
	std::string addr = "stun.l.google.com";
	uint16_t port = 19302;

	bool operator == (const STUNConfiguration& cmp) const
	{
		return addr == cmp.addr && port == cmp.port;
	}
};

struct RemoteAccess
{
	bool bEnabled = false;
	bool bAllowInput = false;

	bool operator == (const RemoteAccess& cmp) const
	{
		return bEnabled == cmp.bEnabled && bAllowInput == cmp.bAllowInput;
	}
};

struct APPConfig
{
	LANConfiguration lan;
	CoordinatorConfiguration coord;
	STUNConfiguration stun;
	RemoteAccess remote;
};

SPP_AUTOREG_START
{
	rttr::registration::class_<APPConfig>("APPConfig")
		.property("lan", &APPConfig::lan)(rttr::policy::prop::as_reference_wrapper)
		.property("coord", &APPConfig::coord)(rttr::policy::prop::as_reference_wrapper)
		.property("stun", &APPConfig::stun)(rttr::policy::prop::as_reference_wrapper)
		.property("remote", &APPConfig::remote)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<LANConfiguration>("LANConfiguration")
		.property("port", &LANConfiguration::port)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<CoordinatorConfiguration>("CoordinatorConfiguration")
		.property("addr", &CoordinatorConfiguration::addr)(rttr::policy::prop::as_reference_wrapper)
		.property("pwd", &CoordinatorConfiguration::pwd)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<STUNConfiguration>("STUNConfiguration")
		.property("addr", &STUNConfiguration::addr)(rttr::policy::prop::as_reference_wrapper)
		.property("port", &STUNConfiguration::port)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<RemoteAccess>("RemoteAccess")
		.property("bEnabled", &RemoteAccess::bEnabled)(rttr::policy::prop::as_reference_wrapper)
		.property("bAllowInput", &RemoteAccess::bAllowInput)(rttr::policy::prop::as_reference_wrapper)
		;
}
SPP_AUTOREG_END