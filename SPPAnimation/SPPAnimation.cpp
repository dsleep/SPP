// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPAnimation.h"
#include "SPPPlatformCore.h"

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/skeleton.h"


SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	uint32_t GetAnimationVersion()
	{
		return 1;
	}	

	void InitializeAnimation()
	{
		AddDLLSearchPath("../3rdParty/ozz-animation/lib");
	}
}