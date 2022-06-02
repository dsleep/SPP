// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPPhysX.h"

#include "PxPhysicsAPI.h"
#include "SPPPlatformCore.h"

SPP_OVERLOAD_ALLOCATORS

using namespace physx;

namespace SPP
{
	uint32_t GetPhysXVersion()
	{
		return 1;
	}

	PxDefaultAllocator		gAllocator;
	PxDefaultErrorCallback	gErrorCallback;

	PxFoundation* gFoundation = NULL;
	PxPhysics* gPhysics = NULL;

	PxPvd* gPvd = NULL;
	
	#define PVD_HOST "127.0.0.1"

	void InitializePhysX()
	{
#if _DEBUG
		AddDLLSearchPath("../3rdParty/PhysX1.4/PhysX/bin/win.x86_64.vc143.mt/debug");
#else
		AddDLLSearchPath("../3rdParty/PhysX1.4/PhysX/bin/win.x86_64.vc143.mt/release");
#endif

		gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);

		SE_ASSERT(gFoundation);

		gPvd = PxCreatePvd(*gFoundation);

		SE_ASSERT(gPvd);

		PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
		gPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

		gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true, gPvd);

		SE_ASSERT(gPhysics);
	}

}