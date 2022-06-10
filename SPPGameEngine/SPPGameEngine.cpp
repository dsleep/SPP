// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGameEngine.h"
#include "SPPFileSystem.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	OEntity::OEntity(const std::string& InName, SPPDirectory* InParent) : OElement(InName, InParent)
	{
	}
	OEnvironment::OEnvironment(const std::string& InName, SPPDirectory* InParent) : OScene(InName, InParent)
	{
	}

	void OEnvironment::Update(float DeltaTime)
	{
		std::list< OEntity* > copyEntities = _entities;

		for (auto& curEntity : copyEntities)
		{
			if (curEntity) //&& !curEntity->IsPendingGC()
			{
				curEntity->Update(DeltaTime);
			}
		}
	}

	uint32_t GetGameEngineVersion()
	{
		return 1;
	}
}


using namespace SPP;

RTTR_REGISTRATION
{
	rttr::registration::class_<OEntity>("OEntity")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OEnvironment>("OEnvironment")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_entities", &OEnvironment::_entities)(rttr::policy::prop::as_reference_wrapper)
		;	
}