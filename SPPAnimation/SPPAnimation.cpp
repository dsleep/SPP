// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPAnimation.h"
#include "SPPPlatformCore.h"
#include "SPPLogging.h"

#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/animation_optimizer.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/blending_job.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/skeleton_utils.h"

#include "SPPJsonUtils.h"
#include "SPPFileSystem.h"
#include "SPPSTLUtils.h"
#include "SPPString.h"
#include "SPPBase64.h"

SPP_OVERLOAD_ALLOCATORS


namespace SPP
{
	LogEntry LOG_ANIM("Animation");

	uint32_t GetAnimationVersion()
	{
		return 1;
	}

	void InitializeAnimation()
	{
		AddDLLSearchPath("../3rdParty/ozz-animation/lib");
	}

	struct OSkeleton::Impl
	{
		std::shared_ptr<ozz::animation::Skeleton> skeleton;
	};

	OSkeleton::OSkeleton(const std::string& InName, SPPDirectory* InParent) : SPPObject(InName, InParent), _impl(new Impl()) { }
	OSkeleton::~OSkeleton() { }

	uint16_t OSkeleton::GetBoneIndex(const std::string& BoneName)
	{
		return MapFindOrFunc(_boneMap, BoneName, []() { return -1; });
	}

	void RecursiveGenerationBones(ozz::animation::offline::RawSkeleton::Joint& CurrentJoint, Json::Value& CurBoneV, std::map<std::string, DTransform> &boneTransformMap)
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

		DTransform transform;
		std::string boneName = nameV.asCString();
		transform.Location = Vector3(CurrentJoint.transform.translation.x, CurrentJoint.transform.translation.y, CurrentJoint.transform.translation.z);
		transform.Rotation = Quarternion(CurrentJoint.transform.rotation.x, CurrentJoint.transform.rotation.y, CurrentJoint.transform.rotation.z, CurrentJoint.transform.rotation.w);
		transform.Scale = Vector3(CurrentJoint.transform.scale.x, CurrentJoint.transform.scale.y, CurrentJoint.transform.scale.z);

		boneTransformMap[boneName] = transform;

		if (!childrenV.isNull() && childrenV.isArray())
		{
			CurrentJoint.children.resize(childrenV.size());

			for (int32_t Iter = 0; Iter < childrenV.size(); Iter++)
			{
				auto childBoneV = childrenV[Iter];
				RecursiveGenerationBones(CurrentJoint.children[Iter], childBoneV, boneTransformMap);
			}
		}
	}

	OSkeleton* LoadSkeleton(const char* FilePath)
	{
		Json::Value JsonScene;
		if (!FileToJson(FilePath, JsonScene))
		{
			return nullptr;
		}

		std::string ParentPath = stdfs::path(FilePath).parent_path().generic_string();
		std::string SkelName = stdfs::path(FilePath).stem().generic_string();

		//////////////////////////////////////////////////////////////////////////////
		// The first section builds a RawSkeleton from custom data.
		//////////////////////////////////////////////////////////////////////////////

		auto oSkeleton = AllocateObject<OSkeleton>(SkelName, nullptr);

		// Creates a RawSkeleton.
		ozz::animation::offline::RawSkeleton raw_skeleton;

		Json::Value nameV = JsonScene.get("name", Json::Value::nullSingleton());
		Json::Value rootBonesV = JsonScene.get("bones", Json::Value::nullSingleton());

		std::map<std::string, DTransform> transformMap;

		if (!rootBonesV.isNull() && rootBonesV.isArray())
		{
			raw_skeleton.roots.resize(rootBonesV.size());

			for (int32_t Iter = 0; Iter < rootBonesV.size(); Iter++)
			{
				auto currentBoneV = rootBonesV[Iter];
				RecursiveGenerationBones(raw_skeleton.roots[Iter], currentBoneV, transformMap);
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
		oSkeleton->_impl->skeleton = builder(raw_skeleton);
			
		auto jointCount = oSkeleton->_impl->skeleton->num_joints();
		auto jointParents = oSkeleton->_impl->skeleton->joint_parents();
		auto jointNames = oSkeleton->_impl->skeleton->joint_names();

		SE_ASSERT(jointNames.size() == transformMap.size() &&
			jointNames.size() == jointParents.size());

		oSkeleton->_bones.resize(jointNames.size());
		oSkeleton->_parentIdxMap.resize(jointParents.size());
		
		for (int32_t Iter = 0; Iter < jointNames.size(); Iter++)
		{
			oSkeleton->_bones[Iter].Name = jointNames[Iter];
			reinterpret_cast<DTransform&>(oSkeleton->_bones[Iter]) = transformMap[oSkeleton->_bones[Iter].Name];
			oSkeleton->_parentIdxMap[Iter] = jointParents[Iter];
			oSkeleton->_boneMap[oSkeleton->_bones[Iter].Name] = Iter;
		}

		return oSkeleton;
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
		Rotation_Z,

		Scale_X,
		Scale_Y,
		Scale_Z
	};

	namespace EAnimLinkFlags
	{
		const uint32_t LocationShift = 0;
		const uint32_t RotationEulerShift = 3;
		const uint32_t RotationQuatShift = 6;
		const uint32_t ScaleShift = 10;

		enum Values
		{
			EmptySet = 0,

			Location_X = 1 << 0,
			Location_Y = 1 << 1,
			Location_Z = 1 << 2,

			RotationEuler_X = 1 << 3,
			RotationEuler_Y = 1 << 4,
			RotationEuler_Z = 1 << 5,

			RotationQuat_X = 1 << 6,
			RotationQuat_Y = 1 << 7,
			RotationQuat_Z = 1 << 8,
			RotationQuat_W = 1 << 9,

			Scale_X = 1 << 10,
			Scale_Y = 1 << 11,
			Scale_Z = 1 << 12,

			LocationValues = Location_X | Location_Y | Location_Z,
			RotationEulerValues = RotationEuler_X | RotationEuler_Y | RotationEuler_Z,
			RotationQuatValues = RotationQuat_X | RotationQuat_Y | RotationQuat_Z | RotationQuat_W,
			ScaleValues = Scale_X | Scale_Y | Scale_Z
		};
	}

	struct TrackLink
	{
		std::string boneName;
		EAnimLinkType linkType;
	};

	bool GetLinkFromString(const std::string& InString, TrackLink& oLink, uint8_t arrayIdx)
	{
		if (StartsWith(InString, "pose.bones"))
		{
			auto boneSplit = std::str_split(InString, '\'');

			if (boneSplit.size() == 3)
			{
				oLink.boneName = boneSplit[1];

				if (EndsWith(InString, "rotation_quaternion"))
				{
					oLink.linkType = (EAnimLinkType)((uint8_t)EAnimLinkType::Quat_X + arrayIdx);
					return true;
				}
				else if (EndsWith(InString, "rotation_euler"))
				{
					oLink.linkType = (EAnimLinkType)((uint8_t)EAnimLinkType::Rotation_X + arrayIdx);
					return true;
				}
				else if (EndsWith(InString, "location"))
				{
					oLink.linkType = (EAnimLinkType)((uint8_t)EAnimLinkType::Location_X + arrayIdx);
					return true;
				}
				else if (EndsWith(InString, "scale"))
				{
					oLink.linkType = (EAnimLinkType)((uint8_t)EAnimLinkType::Scale_X + arrayIdx);
					return true;
				}
			}
		}

		SE_ASSERT(false);

		return false;
	}

	struct BoneTrack
	{
		EAnimLinkType linkType;
		std::vector<Vector2> keys;
	};

	struct BoneTrackSet
	{
		Vector2 Range = Vector2(std::numeric_limits<float>::max(), std::numeric_limits<float>::min());

		std::map< float, uint16_t > flags;
		std::map< float, Vector3 > locations;
		std::map< float, Vector3 > rotations;
		std::map< float, Vector4 > quatRotations;
	};

	void FillTrack(BoneTrackSet& BoneSet, const TrackLink& link, std::vector<Vector2> keys)
	{
		switch (link.linkType)
		{
		case EAnimLinkType::Location_X:
		case EAnimLinkType::Location_Y:
		case EAnimLinkType::Location_Z:
			for (auto& curKey : keys)
			{
				auto typeOffset = (uint8_t)link.linkType - (uint8_t)EAnimLinkType::Location_X;

				auto& curValue = MapFindOrAdd(BoneSet.locations, curKey[0], [] { return Vector3(0, 0, 0); });
				curValue[typeOffset] = curKey[1];

				BoneSet.Range[0] = std::min(BoneSet.Range[0], curKey[0]);
				BoneSet.Range[1] = std::max(BoneSet.Range[1], curKey[0]);

				//BoneSet.flags[curKey[0]] |= (1 << (EAnimLinkFlags::LocationShift + typeOffset));
			}
			break;
		case EAnimLinkType::Scale_X:
		case EAnimLinkType::Scale_Y:
		case EAnimLinkType::Scale_Z:
			// we messing with scale?
			break;
		case EAnimLinkType::Rotation_X:
		case EAnimLinkType::Rotation_Y:
		case EAnimLinkType::Rotation_Z:
			for (auto& curKey : keys)
			{
				auto typeOffset = (uint8_t)link.linkType - (uint8_t)EAnimLinkType::Rotation_X;

				auto& curValue = MapFindOrAdd(BoneSet.rotations, curKey[0], [] { return Vector3(0, 0, 0); });
				curValue[typeOffset] = curKey[1];

				BoneSet.Range[0] = std::min(BoneSet.Range[0], curKey[0]);
				BoneSet.Range[1] = std::max(BoneSet.Range[1], curKey[0]);

				//BoneSet.flags[curKey[0]] |= (1 << (EAnimLinkFlags::RotationEulerShift + typeOffset));
			}
			break;
		case EAnimLinkType::Quat_X:
		case EAnimLinkType::Quat_Y:
		case EAnimLinkType::Quat_Z:
		case EAnimLinkType::Quat_W:
			for (auto& curKey : keys)
			{
				auto typeOffset = (uint8_t)link.linkType - (uint8_t)EAnimLinkType::Quat_X;

				auto& curValue = MapFindOrAdd(BoneSet.quatRotations, curKey[0], [] { return Vector4(0, 0, 0, 0); });
				curValue[typeOffset] = curKey[1];

				BoneSet.Range[0] = std::min(BoneSet.Range[0], curKey[0]);
				BoneSet.Range[1] = std::max(BoneSet.Range[1], curKey[0]);

				//BoneSet.flags[curKey[0]] |= (1 << (EAnimLinkFlags::RotationQuatShift + typeOffset));
			}
			break;
		}
	}

	//////

	struct OAnimation::Impl
	{
		std::shared_ptr<ozz::animation::Animation> animation;
	};

	OAnimation::OAnimation(const std::string& InName, SPPDirectory* InParent) : SPPObject(InName, InParent), _impl(new Impl()) { }
	OAnimation::~OAnimation() { }

	OAnimation* LoadAnimations(const char* FilePath, OSkeleton* referenceSkel)
	{
		Json::Value JsonScene;
		if (!FileToJson(FilePath, JsonScene))
		{
			return nullptr;
		}

		std::string ParentPath = stdfs::path(FilePath).parent_path().generic_string();
		std::string AnimName = stdfs::path(FilePath).stem().generic_string();

		//////////////////////////////////////////////////////////////////////////////
		// The first section builds a RawSkeleton from custom data.
		//////////////////////////////////////////////////////////////////////////////

		auto oAnimation = AllocateObject<OAnimation>(AnimName, nullptr);


		auto& refBones = referenceSkel->GetBoneMap();

		//Json::Value actionsV = JsonScene.get("actions", Json::Value::nullSingleton());

		//if (!actionsV.isNull() && actionsV.isArray())
		{
			//for (int32_t Iter = 0; Iter < actionsV.size(); Iter++)
			{
				//auto curActionV = actionsV[Iter];

				Json::Value nameV = JsonScene.get("name", Json::Value::nullSingleton());
				Json::Value tracksV = JsonScene.get("tracks", Json::Value::nullSingleton());

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
						uint8_t arrayIdx = (uint8_t)dataPathIdxV.asInt();

						TrackLink link;
						bool bDidLink = GetLinkFromString(dataPath, link, arrayIdx);
						SE_ASSERT(bDidLink);
						if (bDidLink)
						{
							auto& boneTrack = MapFindOrAdd(boneTracks, link.boneName);

							auto keyData = base64_decode(DataBlob);
							std::vector<Vector2> keys;
							keys.resize(frameCountV.asInt());
							SE_ASSERT(keyData.size() == sizeof(float) * 2 * keys.size());
							memcpy(keys.data(), keyData.data(), keyData.size());

							FillTrack(boneTrack, link, keys);
						}
					}
				}

				Vector2 TotalRange = Vector2(std::numeric_limits<float>::max(), std::numeric_limits<float>::min());
				for (auto& [key, value] : boneTracks)
				{
					TotalRange[0] = std::min(TotalRange[0], value.Range[0]);
					TotalRange[1] = std::max(TotalRange[1], value.Range[1]);
				}

				float TotalDuration = TotalRange[1] / 60.0f;
				float RangeRescale = (1.0f / 60.0f);

				// track should now be filled
				// 
				// Creates a RawAnimation.
				ozz::animation::offline::RawAnimation raw_animation;

				// Sets animation duration (to 1.4s).
				// All the animation keyframes times must be within range [0, duration].
				const float uglyEpsilon = 0.01f;
				raw_animation.duration = TotalDuration + uglyEpsilon;
				raw_animation.tracks.resize(boneTracks.size());

				//TODO report is only part of location,rotation, etc values were set
				bool reportPartials = true;

				raw_animation.name = nameV.asCString();		
				raw_animation.tracks.resize(refBones.size());

				for (auto& [key, value] : boneTracks)
				{
					auto getBoneIdx = refBones.find(key);
					SE_ASSERT(getBoneIdx != refBones.end());
					uint32_t trackIdx = getBoneIdx->second;

					for (auto& curVal : value.locations)
					{
						auto& valVec = curVal.second;
						const ozz::animation::offline::RawAnimation::TranslationKey newKey = {
							curVal.first * RangeRescale, ozz::math::Float3(valVec[0], valVec[1], valVec[2]) };
						raw_animation.tracks[trackIdx].translations.push_back(newKey);
					}

					for (auto& curVal : value.rotations)
					{
						auto& valVec = curVal.second;
						const ozz::animation::offline::RawAnimation::RotationKey newKey = {
							curVal.first * RangeRescale, ozz::math::Quaternion::FromEuler(valVec[0], valVec[1], valVec[2]) };
						raw_animation.tracks[trackIdx].rotations.push_back(newKey);
					}

					for (auto& curVal : value.quatRotations)
					{
						auto& valVec = curVal.second;
						const ozz::animation::offline::RawAnimation::RotationKey newKey = {
							curVal.first * RangeRescale, ozz::math::Quaternion(valVec[0], valVec[1], valVec[2], valVec[3]) };
						raw_animation.tracks[trackIdx].rotations.push_back(newKey);
					}
				}

				SE_ASSERT(raw_animation.Validate());

				// Creates a AnimationBuilder instance.
				ozz::animation::offline::AnimationBuilder builder;


				//OPTIMIZER
				//ozz::animation::offline::AnimationOptimizer optimizer;
				//ASSERT_TRUE(optimizer(input, *skeleton, &output));

				// Executes the builder on the previously prepared RawAnimation, which returns
				// a new runtime animation instance.
				// This operation will fail and return an empty unique_ptr if the RawAnimation
				// isn't valid.

				oAnimation->_impl->animation = builder(raw_animation);
			}
		}

		return oAnimation;
	}

	bool OAnimation::Matches(OSkeleton* CompareSkeleton) const
	{
		//TODO proper compare
		return true;
	}


	//////

	struct AnimationSampler
	{
		// Blending weight_setting for the layer.
		float weight_setting = 1.0f;

		// Runtime animation.
		std::shared_ptr<ozz::animation::Animation> animation;

		// Sampling context.

		ozz::animation::SamplingJob::Context context;
		ozz::animation::SamplingJob sampling_job;

		// Buffer of local transforms as sampled from animation_.
		ozz::vector<ozz::math::SoaTransform> locals;

		// Per-joint weights used to define the partial animation mask. Allows to
		// select which joints are considered during blending, and their individual
		// weight_setting.
		ozz::vector<ozz::math::SimdFloat4> joint_weights;

		//
		ozz::vector<ozz::math::Float4x4> models_transforms;
	};

	struct OAnimator::Impl
	{
		std::vector< std::shared_ptr< AnimationSampler > > activeAnimations;

		// Buffer of local transforms which stores the blending result.
		std::vector<ozz::math::SoaTransform> blended_locals;

		// Buffer of model space matrices. These are computed by the local-to-model
		// job after the blending stage.
		std::vector<ozz::math::Float4x4> models;
	};

	OAnimator::OAnimator(const std::string& InName, SPPDirectory* InParent) : SPPObject(InName, InParent), _impl(new Impl()) { }
	OAnimator::~OAnimator() { }

	void OAnimator::SetSkeleton(OSkeleton* InSkeleton)
	{
		_skeleton = InSkeleton;
		auto ozzSkel = _skeleton->_impl->skeleton;

		const int num_joints = ozzSkel->num_joints();
		const int num_soa_joints = ozzSkel->num_soa_joints();

		// Allocates local space runtime buffers of blended data.
		_impl->blended_locals.resize(num_soa_joints);
		// Allocates model space runtime buffers of blended data.
		_impl->models.resize(num_joints);
	}

	void OAnimator::AddAnimation(OAnimation* InAnimation)
	{		
		SE_ASSERT(InAnimation->Matches(_skeleton));
		_animTable[InAnimation->_impl->animation->name()] = InAnimation;
	}

	void OAnimator::PlayAnimation(const std::string& AnimName)
	{
		SE_ASSERT(_skeleton);

		auto findAnim = _animTable.find(AnimName);

		if (findAnim == _animTable.end())
		{
			SPP_LOG(LOG_ANIM, LOG_WARNING, "PlayAnimation cant find animation %s", AnimName.c_str());
			return;
		}

		std::shared_ptr<ozz::animation::Skeleton> ozzSkel = _skeleton->_impl->skeleton;

		const int num_joints = ozzSkel->num_joints();
		const int num_soa_joints = ozzSkel->num_soa_joints();

		std::shared_ptr< AnimationSampler > newAnim = std::make_shared< AnimationSampler >();

		// Allocates sampler runtime buffers
		newAnim->locals.resize(num_soa_joints);
		// Allocates per-joint weights used for the partial animation. Note that
		// this is a Soa structure.
		newAnim->joint_weights.resize(num_soa_joints);
		// Allocates a context that matches animation requirements.
		newAnim->context.Resize(num_joints);
		newAnim->models_transforms.resize(num_joints);
				
		newAnim->animation = findAnim->second->_impl->animation;
		newAnim->sampling_job.animation = newAnim->animation.get();
		newAnim->sampling_job.context = &newAnim->context;
		
		//TODO needs to tick this
		newAnim->sampling_job.ratio = 0.5f;
		newAnim->sampling_job.output = ozz::make_span(newAnim->locals);

		if (!newAnim->sampling_job.Run())
		{
			SPP_LOG(LOG_ANIM, LOG_WARNING, "sampling_job failed to run");
		}

		// Converts from local space to model space matrices.
		// Gets the output of the blending stage, and converts it to model space.

		// Setup local-to-model conversion job.
		ozz::animation::LocalToModelJob ltm_job;
		ltm_job.skeleton = ozzSkel.get();
		ltm_job.input = make_span(newAnim->locals);
		ltm_job.output = make_span(newAnim->models_transforms);

		// Run ltm job.
		if (!ltm_job.Run()) 
		{
			SPP_LOG(LOG_ANIM, LOG_WARNING, "ltm_job failed to run");
		}

		_impl->activeAnimations.push_back(newAnim);		
	}
}

using namespace SPP;

RTTR_REGISTRATION
{
	rttr::registration::class_<OSkeleton>("OSkeleton")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		);

	rttr::registration::class_<OAnimation>("OAnimation")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OAnimator>("OAnimator")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_animTable", &OAnimator::_animTable)(rttr::policy::prop::as_reference_wrapper)
		.property("_skeleton", &OAnimator::_skeleton)(rttr::policy::prop::as_reference_wrapper)
		;
}