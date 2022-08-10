// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPAnimation.h"
#include "SPPPlatformCore.h"

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/skeleton.h"

#include "SPPJsonUtils.h"
#include "SPPFileSystem.h"
#include "SPPString.h"

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

	void RecursiveGenerationBones(ozz::animation::offline::RawSkeleton::Joint &CurrentJoint, Json::Value &CurBoneV)
	{
		Json::Value nameV = CurBoneV.get("name", Json::Value::nullSingleton());
		Json::Value boneL = CurBoneV.get("l", Json::Value::nullSingleton());
		Json::Value boneR = CurBoneV.get("r", Json::Value::nullSingleton());
		Json::Value boneS = CurBoneV.get("s", Json::Value::nullSingleton());		
		Json::Value childrenV = CurBoneV.get("children", Json::Value::nullSingleton());
		
		std::vector<std::string> lA = std::str_split(std::string(boneL.asCString()), ' ');
		std::vector<std::string> rA = std::str_split(std::string(boneR.asCString()), ' ');
		std::vector<std::string> sA = std::str_split(std::string(boneS.asCString()), ' ');
		
		CurrentJoint.name = nameV.asCString();
		CurrentJoint.transform.translation = ozz::math::Float3(std::atof(lA[0].c_str()), std::atof(lA[1].c_str()), std::atof(lA[2].c_str()));
		CurrentJoint.transform.rotation = ozz::math::Quaternion(std::atof(rA[0].c_str()), std::atof(rA[1].c_str()), std::atof(rA[2].c_str()), std::atof(rA[3].c_str()));
		CurrentJoint.transform.scale = ozz::math::Float3(std::atof(sA[0].c_str()), std::atof(sA[1].c_str()), std::atof(sA[2].c_str()));

		if (!childrenV.isNull() && childrenV.isArray())
		{
			CurrentJoint.children.resize(childrenV.size());

			for (int32_t Iter = 0; Iter < childrenV.size(); Iter++)
			{
				auto childBoneV = childrenV[Iter];
				RecursiveGenerationBones(CurrentJoint.children[Iter], childBoneV);
			}
		}
	}

	void *LoadSkeleton(const char* FilePath)
	{
		Json::Value JsonScene;
		if (!FileToJson(FilePath, JsonScene))
		{
			return nullptr;
		}

		std::string ParentPath = stdfs::path(FilePath).parent_path().generic_string();
		std::string SimpleSceneName = stdfs::path(FilePath).stem().generic_string();

		//////////////////////////////////////////////////////////////////////////////
		  // The first section builds a RawSkeleton from custom data.
		  //////////////////////////////////////////////////////////////////////////////

		  // Creates a RawSkeleton.
		ozz::animation::offline::RawSkeleton raw_skeleton;

		Json::Value nameV = JsonScene.get("name", Json::Value::nullSingleton());
		Json::Value rootBonesV = JsonScene.get("bones", Json::Value::nullSingleton());

		if (!rootBonesV.isNull() && rootBonesV.isArray())				
		{
			raw_skeleton.roots.resize(rootBonesV.size());

			for (int32_t Iter = 0; Iter < rootBonesV.size(); Iter++)
			{
				auto currentBoneV = rootBonesV[Iter];
				RecursiveGenerationBones(raw_skeleton.roots[Iter], currentBoneV);
			}
		}

		// Test for skeleton validity.
		// The main invalidity reason is the number of joints, which must be lower
		// than ozz::animation::Skeleton::kMaxJoints.
		if (!raw_skeleton.Validate()) {
			return nullptr;
		}

		//////////////////////////////////////////////////////////////////////////////
		// This final section converts the RawSkeleton to a runtime Skeleton.
		//////////////////////////////////////////////////////////////////////////////

		// Creates a SkeletonBuilder instance.
		ozz::animation::offline::SkeletonBuilder builder;

		// Executes the builder on the previously prepared RawSkeleton, which returns
		// a new runtime skeleton instance.
		// This operation will fail and return an empty unique_ptr if the RawSkeleton
		// isn't valid.
		ozz::unique_ptr<ozz::animation::Skeleton> skeleton = builder(raw_skeleton);

		// ...use the skeleton as you want...
		return nullptr;
	}
}