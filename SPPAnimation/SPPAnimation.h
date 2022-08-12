// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPMath.h"
#include "SPPObject.h"

#if _WIN32 && !defined(SPP_ANIMATION_STATIC)

	#ifdef SPP_ANIMATION_EXPORT
		#define SPP_ANIMATION_API __declspec(dllexport)
	#else
		#define SPP_ANIMATION_API __declspec(dllimport)
	#endif

#else

	#define SPP_ANIMATION_API 

#endif

namespace SPP
{		
	SPP_ANIMATION_API uint32_t GetAnimationVersion();
	SPP_ANIMATION_API void InitializeAnimation();
	
	struct BoneWithTransform
	{
		std::string Name;

		Vector3 Location;
		Quarternion Rotation;
		Vector3 Scale;
	};

	class SPP_ANIMATION_API OSkeleton : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND
		friend SPP_ANIMATION_API OSkeleton* LoadSkeleton(const char* FilePath);

		struct Impl;
		std::unique_ptr<Impl> _impl;

	protected:
		OSkeleton(const std::string& InName, SPPDirectory* InParent);
		std::vector< BoneWithTransform > _bones;

	public:	
		virtual ~OSkeleton();
	};


	SPP_ANIMATION_API OSkeleton* LoadSkeleton(const char* FilePath);
	SPP_ANIMATION_API void* LoadAnimations(const char* FilePath);
}
