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
		friend class SPP_ANIMATION_API OAnimator;

		struct Impl;
		std::unique_ptr<Impl> _impl;

	protected:
		OSkeleton(const std::string& InName, SPPDirectory* InParent);
		
		std::map<std::string, uint16_t> _boneMap;
		std::vector< BoneWithTransform > _bones;
		std::vector< uint16_t > _parentIdxMap;

	public:	
		virtual ~OSkeleton();

		uint16_t GetBoneIndex(const std::string& BoneName);

		const std::vector< BoneWithTransform > &GetBones()
		{
			return _bones;
		}
		const std::map<std::string, uint16_t>& GetBoneMap()
		{
			return _boneMap;
		}
	};

	class SPP_ANIMATION_API OAnimation : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND
		
		friend SPP_ANIMATION_API OAnimation* LoadAnimations(const char* FilePath, OSkeleton* referenceSkel);
		friend class SPP_ANIMATION_API OAnimator;

		struct Impl;
		std::unique_ptr<Impl> _impl;

	protected:
		OAnimation(const std::string& InName, SPPDirectory* InParent);

	public:
		bool Matches(OSkeleton* CompareSkeleton) const;
		virtual ~OAnimation();
	};

	class SPP_ANIMATION_API OAnimator : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:

		struct Impl;
		std::unique_ptr<Impl> _impl;

		OSkeleton* _skeleton = nullptr;
		std::map< std::string, OAnimation* > _animTable;

		OAnimator(const std::string& InName, SPPDirectory* InParent);

	public:

		void SetSkeleton(OSkeleton* InSkeleton);
		void AddAnimation(OAnimation* InAnimation);

		//void SetupSamplerLayer()
		void PlayAnimation(const std::string& AnimName);

		virtual ~OAnimator();
	};


	SPP_ANIMATION_API OSkeleton* LoadSkeleton(const char* FilePath);
	SPP_ANIMATION_API OAnimation* LoadAnimations(const char* FilePath, OSkeleton* referenceSkel);
}
