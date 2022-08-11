// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPAnimation.h"
#include "SPPPlatformCore.h"

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/skeleton.h"

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/runtime/animation.h"

#include "SPPJsonUtils.h"
#include "SPPFileSystem.h"
#include "SPPSTLUtils.h"
#include "SPPString.h"
#include "SPPBase64.h"

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
	
	enum class EAnimLinkType
	{
		Location_X = 0,
		Location_Y,
		Location_Z,

		Quat_X,
		Quat_Y,
		Quat_Z,
		Quat_W,

		Rotation_X,
		Rotation_Y,
		Rotation_Z
	};

	struct TrackLink
	{
		std::string boneName;
		EAnimLinkType linkType;
	};

	bool GetLinkFromString(const std::string &InString, TrackLink& oLink, uint8_t arrayIdx)
	{
		if (StartsWith(InString, "pose.bones"))
		{
			auto boneSplit = std::str_split(InString, '\'');

			if (boneSplit.size() == 3)
			{
				oLink.boneName = boneSplit[1];

				if (EndsWith(InString, "rotation_euler"))
				{
					oLink.linkType = (EAnimLinkType)((uint8_t)EAnimLinkType::Rotation_X + arrayIdx);
					return true;
				}
				else if (EndsWith(InString, "location"))
				{
					oLink.linkType = (EAnimLinkType)((uint8_t)EAnimLinkType::Location_X + arrayIdx);
					return true;
				}
			}
		}

		return false;
	}

	struct BoneTrack
	{
		EAnimLinkType linkType;
		std::vector<Vector2> keys;
	};

	struct BoneTrackSet
	{
		std::map< float, Vector3 > locations;
		std::map< float, Vector3 > rotations;
	};

	void FillTrack(BoneTrackSet &BoneSet, const TrackLink &link, std::vector<Vector2> keys)
	{
		switch (link.linkType)
		{
		case EAnimLinkType::Location_X:
		case EAnimLinkType::Location_Y:
		case EAnimLinkType::Location_Z:
			for (auto& curKey : keys)
			{
				auto& curValue = MapFindOrAdd(BoneSet.locations, curKey[0], [] { return Vector3(0, 0, 0); });
				curValue[(uint8_t)link.linkType - (uint8_t)EAnimLinkType::Location_X] = curKey[1];
			}
			break;
		case EAnimLinkType::Rotation_X:
		case EAnimLinkType::Rotation_Y:
		case EAnimLinkType::Rotation_Z:
			for (auto& curKey : keys)
			{
				auto & curValue = MapFindOrAdd(BoneSet.rotations, curKey[0], []{ return Vector3(0, 0, 0); });
				curValue[(uint8_t)link.linkType - (uint8_t)EAnimLinkType::Rotation_X] = curKey[1];
			}
			break;
		}
	}

	void* LoadAnimations(const char* FilePath)
	{
		Json::Value JsonScene;
		if (!FileToJson(FilePath, JsonScene))
		{
			return nullptr;
		}

		Json::Value actionsV = JsonScene.get("actions", Json::Value::nullSingleton());

		if (!actionsV.isNull() && actionsV.isArray())
		{				
			for (int32_t Iter = 0; Iter < actionsV.size(); Iter++)
			{				
				auto curActionV = actionsV[Iter];

				Json::Value nameV = curActionV.get("name", Json::Value::nullSingleton());
				Json::Value tracksV = curActionV.get("tracks", Json::Value::nullSingleton());

				std::map<std::string, BoneTrackSet> boneTracks;

				if (!tracksV.isNull() && tracksV.isArray())
				{
					for (int32_t Iter = 0; Iter < tracksV.size(); Iter++)
					{
						auto curTrackV = tracksV[Iter];

						Json::Value dataPathV = curTrackV.get("dataPath", Json::Value::nullSingleton());
						Json::Value dataPathIdxV = curTrackV.get("dataPathArrayIndex", Json::Value::nullSingleton());
						Json::Value frameCountV = curTrackV.get("frameCount", Json::Value::nullSingleton());
						Json::Value dataV = curTrackV.get("data", Json::Value::nullSingleton());

						std::string DataBlob = dataV.asCString();

						std::string dataPath = dataPathV.asCString();
						uint8_t arrayIdx = (uint8_t) dataPathIdxV.asInt();
						
						TrackLink link;
						bool bDidLink = GetLinkFromString(dataPath, link, arrayIdx);
						SE_ASSERT(bDidLink);
						if (bDidLink)
						{
							auto& boneTrack = MapFindOrAdd(boneTracks,link.boneName);

							auto keyData = base64_decode(DataBlob);
							std::vector<Vector2> keys;
							keys.resize(frameCountV.asInt());
							SE_ASSERT(keyData.size() == sizeof(float) * 2 * keys.size());
							memcpy(keys.data(), keyData.data(), keyData.size());

							FillTrack(boneTrack, link, keys);
						}
					}
				}

				// track should now be filled
				// 
				// Creates a RawAnimation.
				ozz::animation::offline::RawAnimation raw_animation;

				// Sets animation duration (to 1.4s).
				// All the animation keyframes times must be within range [0, duration].
				raw_animation.duration = 1.4f;

				raw_animation.tracks.resize(boneTracks.size());

				for (auto& [key, value] : boneTracks)
				{
					uint32_t trackIdx = 0;

					for (auto& curVal : value.locations)
					{
						auto& valVec = curVal.second;
						const ozz::animation::offline::RawAnimation::RotationKey newKey = {
							curVal.first, ozz::math::Quaternion::FromEuler(valVec[0], valVec[1], valVec[2]) };
						raw_animation.tracks[trackIdx].rotations.push_back(newKey);
					}

					for (auto& curVal : value.rotations)
					{
						auto& valVec = curVal.second;
						const ozz::animation::offline::RawAnimation::RotationKey newKey = {
							curVal.first, ozz::math::Quaternion::FromEuler(valVec[0], valVec[1], valVec[2])};
						raw_animation.tracks[trackIdx].rotations.push_back(newKey);
					}					
				}

				SE_ASSERT(raw_animation.Validate());

				// Creates a AnimationBuilder instance.
				ozz::animation::offline::AnimationBuilder builder;

				// Executes the builder on the previously prepared RawAnimation, which returns
				// a new runtime animation instance.
				// This operation will fail and return an empty unique_ptr if the RawAnimation
				// isn't valid.
				ozz::unique_ptr<ozz::animation::Animation> animation = builder(raw_animation);
			}
		}


		return nullptr;
	}
}