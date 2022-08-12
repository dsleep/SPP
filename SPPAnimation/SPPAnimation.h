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
	
	struct DTransform
	{
		Vector3 Location;
		Quarternion Rotation;
		Vector3 Scale;
	};

	struct BoneWithTransform : public DTransform
	{
		std::string Name;
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
		
		std::map<std::string, uint16_t> _boneMap;
		std::vector< BoneWithTransform > _bones;
		std::vector< uint16_t > _parentIdxMap;

	public:	
		virtual ~OSkeleton();

		const std::vector< BoneWithTransform > &GetBones()
		{
			return _bones;
		}
		const std::map<std::string, uint16_t>& GetBoneMap()
		{
			return _boneMap;
		}
	};


	SPP_ANIMATION_API OSkeleton* LoadSkeleton(const char* FilePath);
	SPP_ANIMATION_API void* LoadAnimations(const char* FilePath, OSkeleton* referenceSkel);
}
