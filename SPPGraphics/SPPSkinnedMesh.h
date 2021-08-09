// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPMesh.h"
#include "SPPSTLUtils.h"

namespace SPP
{

	using BoneIdsType = Vector4i;
	using BoneWeightsType = Vector4;

	struct BoneWeightData
	{
		std::vector < BoneIdsType > boneIndices;
		std::vector < BoneWeightsType > boneWeights;
	};

	enum class EulerRotationMode
	{
		QUATERNION_WXYZ = 0,
		EULER_XYZ = 1,  // 1
		EULER_XZY = 2,  // 2
		EULER_YXZ = 3,  // 3
		EULER_YZX = 4,  // 4
		EULER_ZXY = 5,  // 5
		EULER_ZYX = 6  // 6
		//AXIS_ANGLE = -1       // (these are used by Blender too)
		//
	};

	struct BoneAnimationInfo
	{
		struct Vector3Key
		{
			Vector3 value;
			float time;
		};
		struct QuaternionKey
		{
			Quarternion value;
			float time;
		};
		enum BehaviorEnum
		{
			AB_DEFAULT = 0,		//The value from the default node transformation is taken. 
			AB_CONSTANT = 1,	//The nearest key value is used without interpolation. 
			AB_LINEAR = 2,		//The value of the nearest two keys is linearly extrapolated for the current time value.
			AB_REPEAT = 3,		//The animation is repeated. 
			AB_COUNT			//If the animation key go from n to m and the current time is t, use the value at (t-n) % (|m-n|). 				
		};
		std::vector <Vector3Key> translationKeys;
		std::vector <QuaternionKey> rotationKeys;
		std::vector <Vector3Key> scalingKeys;
		bool tkAndRkHaveUniqueTimePerKey;
		bool tkAndSkHaveUniqueTimePerKey;
		bool rkAndSkHaveUniqueTimePerKey;
		BehaviorEnum preState;	//Defines how the animation behaves before the first key is encountered. 
		BehaviorEnum postState;	//Defines how the animation behaves after the last key was processed. 
		void reset()
		{
			translationKeys.clear(); rotationKeys.clear(); scalingKeys.clear();
			tkAndRkHaveUniqueTimePerKey = tkAndSkHaveUniqueTimePerKey = rkAndSkHaveUniqueTimePerKey = false;
			preState = postState = AB_DEFAULT;
		}
	};

	struct BoneInfo
	{
		std::string boneName;

		unsigned index;		// inside m_BoneInfos vector (bad design)
		unsigned indexMirror;   // .blend only (when not set is == index)
		Matrix4x4 boneOffset;	//Problem: this depends on the mesh index (although I never experienced any problems so far)
		Matrix4x4 boneOffsetInverse;	//Never used, but useful to display skeleton (from bone space to global space)
		//Mat4 globalSpaceBoneOffset;	//Never used
		bool isDummyBone;	// Dummy bones have boneOffset =  boneOffsetInverse = mat4(1), and are not counted in getNumBones()
		bool isUseless;		// Dummy nodes can be marked as useless they could be erased by m_boneInfos, but it's too difficult to do it (see the references stored below)

		//---new--------------------------
		Matrix4x4 preTransform;			// Must always be applied (if preTransformIsPresent == false) before all other transforms
		bool preTransformIsPresent;	// When false preTransform = glm::mat(1)
		Matrix4x4 preAnimationTransform;				// When animation keys are simplyfied some of them (usually translation only) are inglobed in this transform.
		bool preAnimationTransformIsPresent;
		Matrix4x4 multPreTransformPreAnimationTransform;	// just to speed up things a bit               
		Matrix4x4 postAnimationTransform;				// When animation keys are simplyfied some of them (usually rotation and scaling) are inglobed in this transform.
		bool postAnimationTransformIsPresent;

		EulerRotationMode eulerRotationMode;  // Optional (default=0);

		Matrix4x4 transform;				// Must be used when no animation is present, and replaced by the BoneAnimation combined transform otherwise.
		
		using MapUnsignedBoneAnimationInfo = std::map<uint32_t, BoneAnimationInfo>;

		MapUnsignedBoneAnimationInfo boneAnimationInfoMap;
		inline const BoneAnimationInfo* getBoneAnimationInfo(unsigned animationIndex) const
		{
			return MapFindOrNull(boneAnimationInfoMap, animationIndex);
		}
		inline BoneAnimationInfo* getBoneAnimationInfo(unsigned animationIndex)
		{
			return MapFindOrNull(boneAnimationInfoMap, animationIndex);
		}
		//---------------------------------

		// WARNING: Here we store POINTERS to other elements of the SAME vector m_bneInfos:
		// This mean that the vector must stay the same (no grow, and no deletions of elements at all) all the time
		BoneInfo* parentBoneInfo;
		std::vector < BoneInfo* > childBoneInfos;

		Matrix4x4 finalTransformation;
	};

	struct Armature
	{
		std::string ArmatureName;

		std::vector<BoneInfo> boneInfos;
		std::map<std::string, uint32_t> boneIndexMap;
		uint32_t numValidBones = 0;
		std::vector<BoneInfo*> rootBoneInfos;
	};

	class SPP_GRAPHICS_API SkinnedMesh : public Mesh
	{
	protected:

	public:

	};

	class SPP_GRAPHICS_API RenderableSkinnedMesh : public RenderableMesh
	{
	protected:
		std::vector< std::shared_ptr<MeshElement> > _meshElements;

	public:
		void SetMeshData(const std::vector< std::shared_ptr<MeshElement> >& InMeshData)
		{
			_meshElements = InMeshData;
		}
	};

	//SPP_GRAPHICS_API std::shared_ptr<RenderableSkinnedMesh> CreateRenderableSkinnedMesh(bool bIsStatic = false);
}