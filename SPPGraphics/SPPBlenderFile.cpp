// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPBlenderFile.h"
#include "SPPSerialization.h"
#include "SPPAssetCache.h"
#include "SPPMeshlets.h"
#include "SPPMeshSimplifying.h"
#include "SPPLogging.h"
#include "SPPSTLUtils.h"
#include "SPPSkinnedMesh.h"

#include <functional>
#include <unordered_set>
#include <cstdio>

#define FBTBLEND_IMPLEMENTATION
#include "fbtBlend.h"

namespace SPP
{
	LogEntry LOG_BLENDER("BLENDER");
		
	enum BlenderObjectType {
		BL_OBTYPE_EMPTY = 0,
		BL_OBTYPE_MESH = 1,
		BL_OBTYPE_CURVE = 2,
		BL_OBTYPE_SURF = 3,
		BL_OBTYPE_FONT = 4,
		BL_OBTYPE_MBALL = 5,
		BL_OBTYPE_LAMP = 10,
		BL_OBTYPE_CAMERA = 11,
		BL_OBTYPE_WAVE = 21,
		BL_OBTYPE_LATTICE = 22,
		BL_OBTYPE_ARMATURE = 25
	};


#define INVERTYZ true

	namespace BlenderHelper
	{
		inline static const Quarternion& GetInvYZQuat()
		{
			static Quarternion q1;
			static bool firstTime = true;
			if (firstTime) {
				firstTime = false;
				q1 = Quarternion(AxisAngle((float)M_PI / 2, Vector3(1, 0, 0)));
			}
			return q1;
		}
		inline static const Matrix3x3& GetInvYZMatrix3()
		{
			static Matrix3x3 mInvYZ;
			static bool firstTime = true;
			if (firstTime) {
				firstTime = false;
				mInvYZ = GetInvYZQuat().toRotationMatrix();
			}
			return mInvYZ;
		}
		inline static const Matrix4x4& GetInvYZMatrix4()
		{
			static Matrix4x4 mInvYZ;
			static bool firstTime = true;
			if (firstTime) {
				firstTime = false;
				mInvYZ = Matrix4x4::Identity();
				mInvYZ.block<3, 3>(0, 0) = GetInvYZMatrix3();
			}
			return mInvYZ;
		}
		inline static Vector3 ToVector3(const float v[3], bool invertYZ = INVERTYZ)
		{
			return (invertYZ ? (Vector3(v[0], v[1], v[2]) * GetInvYZMatrix3()) : Vector3(v[0], v[1], v[2]));
		}
		inline static void ToMatrix4(Matrix4x4& mOut, const float m[4][4], bool invertYZ = INVERTYZ)
		{
			if (invertYZ)
			{
				Matrix3x3 tmp;
				for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) tmp(i, j) = (float)m[i][j];
				tmp = GetInvYZMatrix3() * tmp;
				for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) mOut(i, j) = tmp(i, j);

				Vector3 orig = Vector3(m[3][0], m[3][1], m[3][2]) * GetInvYZMatrix3();
				for (int i = 0; i < 4; i++) mOut(i, 3) = 0;//(btScalar) m[i][3];
				//for (int j=0;j<4;j++) mOut[3][j] = 0;//(btScalar) m[3][j];
				for (int j = 0; j < 3; j++) mOut(3, j) = orig[j];
				mOut(3, 3) = 1;
			}
			else for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
			{
				mOut(i, j) = (float)m[i][j];
			}
		}
		inline static void ToMatrix4(Matrix4x4& mOut, const float m[3][3], const Vector3& translation, bool invertYZ = INVERTYZ)
		{
			if (invertYZ)
			{
				Matrix4x4 tmp = Matrix4x4::Identity();
				for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) tmp(i, j) = (float)m[i][j];
				tmp = GetInvYZMatrix4() * tmp;
				for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) mOut(i, j) = tmp(i, j);

				Vector3 orig = translation * GetInvYZMatrix3();
				for (int i = 0; i < 3; i++) mOut(i, 3) = 0;
				for (int j = 0; j < 3; j++) mOut(3, j) = orig[j];
				mOut(3, 3) = 1;
			}
			else {
				for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) mOut(i, j) = (float)m[i][j];
				for (int i = 0; i < 3; i++) mOut(i, 3) = 0;//translation[i];
				//for (int j=0;j<4;j++) mOut[3][j] = 0;//(btScalar) m[3][j];
				for (int j = 0; j < 3; j++) mOut(3, j) = translation[j];
				mOut(3, 3) = 1;
			}
		}

		static const Blender::bArmature* GetArmature(const Blender::Object* ob)
		{
			return ((ob && ob->type == (short)BL_OBTYPE_ARMATURE) ? (const Blender::bArmature*)ob->data : NULL);
		}

		struct DeformVertStruct {
			int id;
			float w;
			DeformVertStruct(int _id = 0, float _w = 0) : id(_id), w(_w) {}
		};
		struct DeformVertStructCmp {
			inline bool operator()(const DeformVertStruct& a, const DeformVertStruct& b) const {
				return a.w > b.w;
			}
		};

		inline static void FillDeformGroups(const Blender::MDeformVert& d,
			BoneIdsType& idsOut,
			BoneWeightsType& weightsOut,
			std::map<int, int>* pDeformGroupBoneIdMap,
			int maxNumBonesPerVertex = 4)
		{
			idsOut[0] = idsOut[1] = idsOut[2] = idsOut[3] = 0;
			weightsOut[0] = weightsOut[1] = weightsOut[2] = weightsOut[3] = 0;
			static std::vector<DeformVertStruct> dvs;
			static std::map<int, int>::const_iterator it;
			if (d.dw && d.totweight > 0)
			{
				dvs.resize(d.totweight);
				for (int j = 0; j < d.totweight; j++)
				{
					const Blender::MDeformWeight& dw = d.dw[j];
					DeformVertStruct& ds = dvs[j];
					if (pDeformGroupBoneIdMap)
					{
						if ((it = pDeformGroupBoneIdMap->find(dw.def_nr)) != pDeformGroupBoneIdMap->end())
						{
							ds.id = it->second;
						}
						else
						{
							SPP_LOG(LOG_BLENDER, LOG_INFO, "Error: Deform Group Index: %d not found in pDeformGroupBoneIdMap", dw.def_nr);
							ds.id = -1;
						}
					}
					else ds.id = dw.def_nr;
					ds.w = dw.weight;
				}
				std::sort(dvs.begin(), dvs.end(), DeformVertStructCmp());
				maxNumBonesPerVertex = maxNumBonesPerVertex > 4 ? 4 : maxNumBonesPerVertex;
				dvs.resize(maxNumBonesPerVertex); float totWeight = 0;
				for (int i = 0; i < maxNumBonesPerVertex; i++)
				{
					const DeformVertStruct& ds = dvs[i];
					idsOut[i] = ds.id;
					weightsOut[i] = ds.w;
					totWeight += ds.w;
				}
				if (totWeight > 0)
				{
					for (int i = 0; i < maxNumBonesPerVertex; i++)
					{
						weightsOut[i] /= totWeight;
					}
				}
			}
			//if (d.flag!=0) fbtPrintf("\tflag=%1d", d.flag);
		}
	}

	inline static bool IsDummyBone(const Blender::Bone* b)
	{
		//return false;   // We DON'T support Dummy Bones with .blend files...
		//return (b && b->flag == 12303);  // TODO: check better ( my dummy 'root' flag:12303 Other non-dummy values: 8208,8192,8320,8336 ))
		//TODO FIX ME?!
		const std::string name = b->name;// String::ToLower(string(b->name));
		//if (!name.find("def")==string::npos) return true;   // to remove
		if (name.find(".ik.") != std::string::npos) return true;
		if (name.find(".link.") != std::string::npos) return true;
		if (name.find(".rev.") != std::string::npos) return true;
		return false;
	}



	inline static int GetNumChildBones(const Blender::Bone* b)
	{
		int rv = 0;
		if (!b) return rv;
		b = (const Blender::Bone*)b->childbase.first;
		while (b) {
			++rv;
			b = b->next;
		}
		return rv;
	}
	inline static bool IsGoodBone(const Blender::Bone* b)
	{
		return b && (b->parent || GetNumChildBones(b) > 0);
	}

	inline static bool Exists(const std::string& name, std::vector<std::string>& v, bool addIfNonExistent)
	{
		const std::vector<std::string>::const_iterator it = std::find(v.begin(), v.end(), name);
		const bool exists = (it != v.end());
		if (addIfNonExistent && !exists) v.push_back(name);
		return exists;
	}

	static void _AddAllBonesFromTheFirstRootBone(std::vector<BoneInfo>& ar,
		const Blender::Bone* b,
		std::map<const Blender::Bone*, int>* pInternalBoneMap,
		unsigned& numNextValidBoneInsideBoneInfos, unsigned& numDummyBones,
		bool invertYZ = INVERTYZ, std::vector<std::string>* palreadyAddedBones = NULL)
	{
		if (!b) return;
		if (palreadyAddedBones && Exists(std::string(b->name), *palreadyAddedBones, true)) return;
		if (IsGoodBone(b))
		{
			const bool isDummyBone = IsDummyBone(b);

#define USE_SAFETY_CHECKS
#ifdef USE_SAFETY_CHECKS
			if (ar.size() == 0) {
				SPP_LOG(LOG_BLENDER, LOG_INFO, "Error: _AddAllBonesFromTheFirstRootBone(...) in 'meshBlender.cpp' must be called with ar.size()>0.");
				return;
			}
			if (isDummyBone) {
				if (ar.size() - 1 < numDummyBones) {
					SPP_LOG(LOG_BLENDER, LOG_INFO, "Error: _AddAllBonesFromTheFirstRootBone(...) in 'meshBlender.cpp' has: ar.size()-1(=%d) < numDummyBones(%d) for bone: '%s'.", (int)ar.size() - 1, (int)numDummyBones, b->name);
					return;
				}
			}
			else if (ar.size() <= numNextValidBoneInsideBoneInfos) {
				SPP_LOG(LOG_BLENDER, LOG_INFO, "Error: _AddAllBonesFromTheFirstRootBone(...) in 'meshBlender.cpp' has: ar.size()(=%d) <= numNextValidBoneInsideBoneInfos(%d) for bone: '%s'.", (int)ar.size(), (int)numNextValidBoneInsideBoneInfos, b->name);
				return;
			}
#undef USE_SAFETY_CHECKS
#endif //USE_SAFETY_CHECKS

			const unsigned boneIndexToFillInsideAr = isDummyBone ? (ar.size() - (++numDummyBones)) : (numNextValidBoneInsideBoneInfos++);//ar.size();
			//ar.push_back(Mesh::BoneInfo());
			if (pInternalBoneMap) (*pInternalBoneMap)[b] = boneIndexToFillInsideAr;

			BoneInfo& bone = ar[boneIndexToFillInsideAr];
			bone.index = bone.indexMirror = boneIndexToFillInsideAr;
			if (strlen(b->name) > 0) bone.boneName = std::string(b->name);
			//else bone.boneName="";
			bone.eulerRotationMode = EulerRotationMode::EULER_XYZ;//0;//glm::EULER_YZX;//b->layer;//b->flag;       // It will be overwritten later (if this info is available), and this variable won't break quaternion-based keyframes: so I think I can set it here to the most common Euler mode (or not?)
			//printf("boneName: %s        bone.eulerRotationMode: %d\n",bone.boneName.c_str(),/*bone.eulerRotationMode,*/b->flag);
			bone.isDummyBone = isDummyBone;
			bone.isUseless = false;         // we don't use it for Blend files (or maybe we'll set these later)

			//if (bone.isDummyBone)  bone.boneOffsetInverse = bone.boneOffset = glm::mat4(1);    // Not used in dummy bones
			//else
			{
				BlenderHelper::ToMatrix4(bone.boneOffsetInverse, b->arm_mat, invertYZ);
				bone.boneOffset = bone.boneOffsetInverse.inverse();
			}

			bone.preTransform = Matrix4x4::Identity();
			bone.preTransformIsPresent = false;
			//bone.preAnimationTransform = glm::mat4(1);                                                              // [*] Used only when animation is present
			//bone.preAnimationTransformIsPresent = false;

			/*
			bone->arm_mat is (bonemat(b)+head(b))*arm_mat(b-1), so it is in object_space.
			bone_mat is the rotation derived from head/tail/roll
			pose_mat(b) = pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b) , therefore pose_mat is object space.
			*/
			/*
			# parent head to tail, and parent tail to child head
			bone_translation = Matrix.Translation(Vector((0, bone.parent.length, 0)) + bone.head)
			# parent armature space matrix, translation, child bone rotation
			bone.matrix_local == bone.parent.matrix_local * bone_translation * bone.matrix.to_4x4()
			*/

			//== THIS SEEMS TO WORK =================================================================
			// For bones without any animation keyframe (or using manual animation)
			Vector3 boneTranslation(0, 0, 0);
			if (b->parent) {
				//if (!IsDummyBone(b->parent))
				boneTranslation[1] += b->parent->length;
			}
			boneTranslation += BlenderHelper::ToVector3(b->head);

			BlenderHelper::ToMatrix4(bone.transform, b->bone_mat, boneTranslation, invertYZ);

			// Now for animations:
			BlenderHelper::ToMatrix4(bone.preAnimationTransform, b->bone_mat, boneTranslation, invertYZ);
			bone.preAnimationTransformIsPresent = true;
			//=======================================================================================


			bone.multPreTransformPreAnimationTransform = bone.preTransform * bone.preAnimationTransform;
			bone.postAnimationTransform = Matrix4x4::Identity();                                                             // [*] Used only when animation is present (after translation key)
			bone.postAnimationTransformIsPresent = false;

			// For reference: all the data in the Blender::Bone struct--------------
			/*
			bone.roll = b->roll;
			bone.head = BlenderHelper::ToVector3(b->head,invertYZ);            // (*)
			bone.tail = BlenderHelper::ToVector3(b->tail,invertYZ);            // (*)
			BlenderHelper::ToBtMatrix3x3(bone.bone_mat,b->bone_mat,invertYZ);    // (*)
			bone.flag = b->flag;
			bone.arm_roll = b->arm_roll;
			bone.arm_head = BlenderHelper::ToVector3(b->arm_head,invertYZ);    // (*)
			bone.arm_tail = BlenderHelper::ToVector3(b->arm_tail,invertYZ);    // (*)
			BlenderHelper::ToBtMatrix4x4(bone.arm_mat,b->arm_mat,invertYZ);      // (*)
			bone.dist = (btScalar) b->dist;
			bone.weight = (btScalar) b->weight;
			bone.xwidth = (btScalar) b->xwidth;
			bone.length = (btScalar) b->length;
			bone.zwidth = (btScalar) b->zwidth;
			bone.ease1 = (btScalar) b->ease1;
			bone.ease2 = (btScalar) b->ease2;
			bone.rad_head = (btScalar) b->rad_head;
			bone.rad_tail = (btScalar) b->rad_tail;
			BlenderHelper::ToVector3(b->size,invertYZ);                        // (*)
			bone.layer = b->layer;
			bone.segments = b->segments;
			*/
			//BlenderHelper::Display(b);
			//------------------------------------------------------------------------
		}
		if (!b->parent) {
			// add siblings
			const Blender::Bone* s = b->next;
			while (s) {
				_AddAllBonesFromTheFirstRootBone(ar, s, pInternalBoneMap, numNextValidBoneInsideBoneInfos, numDummyBones, invertYZ, palreadyAddedBones);
				s = s->next;
			}
		}
		const Blender::Bone* c = (const Blender::Bone*)b->childbase.first;
		while (c) {
			_AddAllBonesFromTheFirstRootBone(ar, c, pInternalBoneMap, numNextValidBoneInsideBoneInfos, numDummyBones, invertYZ, palreadyAddedBones);
			c = c->next;
		}
	}

	static void _GetNumBonesFromTheFirstRootBone(const Blender::Bone* b, int& numBonesOut, std::vector<std::string>* palreadyAddedBones = NULL)
	{
		if (!b) return;
		if (palreadyAddedBones && Exists(std::string(b->name), *palreadyAddedBones, true)) return;
		if (IsGoodBone(b)) ++numBonesOut;
		if (!b->parent)
		{
			// add siblings
			const Blender::Bone* s = b->next;
			while (s) {
				_GetNumBonesFromTheFirstRootBone(s, numBonesOut, palreadyAddedBones);
				s = s->next;
			}
		}
		// add children
		const Blender::Bone* c = (const Blender::Bone*)b->childbase.first;
		while (c)
		{
			_GetNumBonesFromTheFirstRootBone(c, numBonesOut, palreadyAddedBones);
			c = c->next;
		}
	}
		
	static void LoadArmature(const Blender::Object* armatureObject,
		std::vector<BoneInfo>& boneInfos,
		std::map<std::string, uint32_t>& boneIndexMap,
		uint32_t& numValidBones,
		std::vector<BoneInfo*>& rootBoneInfos,
		bool invertYZ = INVERTYZ)
	{
		const Blender::bArmature* a = BlenderHelper::GetArmature(armatureObject);
		if (!a)
		{
			return;
		}
		std::map<const Blender::Bone*, int> internalBoneMap;

		//load bones here:-------------------------------------------------
		const Blender::ListBase rootBones = a->bonebase;   // These seems to be the root bones only...
		const Blender::Bone* b = (const Blender::Bone*)rootBones.first;
		std::vector<std::string> alreadyAddedBones;
		int numBones = 0;
		_GetNumBonesFromTheFirstRootBone(b, numBones, &alreadyAddedBones);

		SPP_LOG(LOG_BLENDER, LOG_INFO, "LoadArmature bone count %d", numBones)

			boneInfos.resize(numBones);
		alreadyAddedBones.clear();//printf("Resized %d bones.\n",numBones);
		unsigned numDummyBones = 0;
		// After this call dummy bones are all at the end, and 'mesh.numValidBones' and 'numDummyBones' should be set (but parent/child relations are still missing)
		_AddAllBonesFromTheFirstRootBone(boneInfos, b, &internalBoneMap, numValidBones, numDummyBones, invertYZ, &alreadyAddedBones);
		//printf("Filled %d bones. numValidBones=%d numDummyBones=%d\n",(int)ar.size(),(int) mesh.numValidBones,(int) numDummyBones);

		// Fill mesh.m_boneIndexMap now:
		boneIndexMap.clear();
		for (unsigned i = 0, sz = boneInfos.size(); i < sz; i++)
		{
			const BoneInfo& bone = boneInfos[i];
			//if (i < mesh.numValidBones)   // Nope: better leave the full map
			boneIndexMap[bone.boneName] = bone.index;
			//printf("boneInfo[%d].boneName='%s'\n",bone.index,bone.boneName.c_str());
		}

		// Second Pass to fill the remaining data (parent/child/mirror fields)-

		// For mirrorBoneId:
		//static std::string sym1[] = 
		//{ 
		//	".L",".Left",".left","_L","_Left","_left","-L","-Left","-left","Left","left"      // The last 2 could be commented out...
		//};
		//static std::string sym2[] =
		//{ 
		//	".R",".Right",".right","_R","_Right","_right","-R","-Right","-right","Right","right"    // The last 2 could be commented out...
		//};
		//static const int symSize = sizeof(sym1) / sizeof(sym1[0]);
		//if (sizeof(sym1) / sizeof(sym1[0]) != sizeof(sym2) / sizeof(sym2[0])) fprintf(stderr, "Error in LoadArmature(...): sizeof(sym1)/sizeof(sym1[0])!=sizeof(sym2)/sizeof(sym2[0])");

		for (std::map<const Blender::Bone*, int>::const_iterator it = internalBoneMap.begin(); it != internalBoneMap.end(); ++it)
		{
			b = it->first;
			BoneInfo& bone = boneInfos[it->second];
			//---------------------
			if (b->parent) {
				auto boneIdx = MapFindOrNull(boneIndexMap, std::string(b->parent->name));
				bone.parentBoneInfo = boneIdx ? &boneInfos[*boneIdx] : nullptr;
				/*
				if (bone.parentBoneInfo)   {
					// This might happen if it's a dummy bone
					// TODO: however I should probably multiply some matrices in between...
					const Blender::Bone* bb = b->parent;
					while ((bb=bb->parent)!=NULL)   {
						bone.parentBoneInfo = (Mesh::BoneInfo*) mesh.getBoneInfoFromName(string(bb->name));
						if (bone.parentBoneInfo) break;
					}
				}
				*/
			}
			else bone.parentBoneInfo = NULL;
			if (bone.parentBoneInfo == NULL)
			{
				rootBoneInfos.push_back(&bone);
				//printf("root bone.name=%s\n",bone.boneName.c_str());
			}


			//printf("bone.name=%s b->name=%s parentId=%d\n",bone.name.c_str(),b->name,bone.parentBoneId);
			const Blender::Bone* c = (const Blender::Bone*)b->childbase.first;
			while (c)
			{
				//printf("c->name=%s\n",c->name);
				auto boneIdx = MapFindOrNull(boneIndexMap, std::string(c->name));
				BoneInfo* childBoneInfo = boneIdx ? &boneInfos[*boneIdx] : nullptr;
				if (!childBoneInfo) {
					fprintf(stderr, "Error: Wrong bone child id of bone %s: -> %s\n", bone.boneName.c_str(), c->name);
					// TODO: This might happen if it's a dummy bone
					// TODO: however I should probably multiply some matrices in between...
				}
				else bone.childBoneInfos.push_back(childBoneInfo);
				//---------------------------------
				c = c->next;
			}
			// Mirror boneId:----------------------
			//const std::string& name = bone.boneName;
			//std::string symName = "";
			//for (int j = 0; j < symSize; j++) {
			//	if (EndsWith(name, sym1[j])) {
			//		symName = name.substr(0, name.size() - sym1[j].size()) + sym2[j];
			//		break;
			//	}
			//	if (EndsWith(name, sym2[j])) {
			//		symName = name.substr(0, name.size() - sym2[j].size()) + sym1[j];
			//		break;
			//	}
			//}
			//if (symName.size() > 0) {
			//	auto boneIdx = MapFindOrNull(boneIndexMap, symName);
			//	const BoneInfo* morrorBoneInfo = boneIdx ? &boneInfos[*boneIdx] : nullptr;
			//	if (morrorBoneInfo) bone.indexMirror = morrorBoneInfo->index;
			//	else fprintf(stderr, "Error: mirror of bone %s should be: %s, but I can't find a bone named like that.\n", name.c_str(), symName.c_str());
			//}
			//-----------------------------------------------------------
		}

		// Now we see if we can remove dummy bones:---- OPTIONAL STEP (comment it out in case of problems) ----------------------------------------
		bool m_boneInfosModified = false;
		if (boneInfos.size() > numValidBones) {
			for (unsigned i = numValidBones; i < boneInfos.size(); i++) {
				BoneInfo& bi = boneInfos[i];
				if (bi.isDummyBone)	// && (!bi.parentBoneInfo || !bi.parentBoneInfo->isDummyBone))
				{
					//if (Mesh::AllDescendantsAreDummy(bi)) {
					//	bi.isUseless = true;
					//	continue;
					//}
				}
			}
		}
		//------------------------------------------------------------------------------------------------------------------------------------------

		{
			boneIndexMap.clear();
			for (unsigned i = 0, sz = boneInfos.size(); i < sz; i++)
			{
				BoneInfo& bi = boneInfos[i];
				if (bi.index != i) {
					printf("Safety check: m_boneInfos[%d].index = %d != %d ->corrected ('%s').\n", i, bi.index, i, bi.boneName.c_str());
					bi.index = i;
				}
				if (i < numValidBones)
				{
					auto it = boneIndexMap.find(bi.boneName);
					if (it != boneIndexMap.end()) {
						printf("Safety check ERROR: m_boneInfos[%d].boneName = m_boneInfos[%d].boneName = '%s'\n", i, it->second, bi.boneName.c_str());
						//TODO: replace bi.boneName here to a unique name
					}
					boneIndexMap[bi.boneName] = i;
				}
			}
		}

		// Now we fill the m_boneInfos::eulerRotationMode-------------
		const Blender::bPose* pose = armatureObject->pose;
		if (pose)
		{
			const Blender::bPoseChannel* pc = (const Blender::bPoseChannel*)pose->chanbase.first;
			while (pc) {
				if (strlen(pc->name) > 0) {
					const std::string name = pc->name;
					if (auto it = boneIndexMap.find(name); it != boneIndexMap.end())
					{
						BoneInfo& bone = boneInfos[it->second];
						bone.eulerRotationMode = (EulerRotationMode)pc->rotmode;
						//printf("set m_boneInfos[%s] = %d\n",bone.boneName.c_str(),bone.eulerRotationMode);

						//#define TEST_BONE_CONSTRAINTS
//#ifdef TEST_BONE_CONSTRAINTS
//						fprintf(stderr, "m_boneInfos[%s] = %d\n", bone.boneName.c_str(), bone.eulerRotationMode);
//						BlenderHelper::CycledCall<Blender::bConstraint>(pc->constraints, &BlenderHelper::Display);
//#undef TEST_BONE_CONSTRAINTS
//#endif //TEST_BONE_CONSTRAINTS

					}
				}

				//------------
				pc = pc->next;
			}
		}
		//------------------------------------------------------------

		// TODO: test armatureObject->poselib-------------------------
		/*
		const Blender::bAction* poselib = armatureObject->poselib;
		if (poselib)   {
			BlenderHelper::Display(poselib);    // Actually poseLib is stored as a simple action!
		}
		*/
		//------------------------------------------------------------


		// TESTING STUFF----------------------------------------------
		/* // CRASHES!
		const Blender::Object* o = (const Blender::Object*) a;  // Is this legal ?
		if (o->constraints.first) {
			fbtPrintf("\tarmature->constraints present\n");
			const Blender::bConstraintTarget* c = (const Blender::bConstraintTarget*) o->constraints.first;
			while (c) {
				BlenderHelper::Display(c);
				c=c->next;
			}
		}
		if (o->nlastrips.first) {
			fbtPrintf("\tarmature->nlastrips present\n");
			const Blender::NlaStrip* s = (const Blender::NlaStrip*) o->nlastrips.first;
			while (s) {
				BlenderHelper::Display(s);
				s=s->next;
			}
		}
		*/
		//------------------------------------------------------------

	}

	struct AnimationInfo
	{
		std::string name;
		float duration;
		float ticksPerSecond;
		float numTicks;
	};

	inline static void GetTimeAndValue(const Blender::BezTriple& bt, float& time, float& value)
	{
		/* vec in BezTriple looks like this: vec[?][0] => frame of the key, vec[?][1] => actual value, vec[?][2] == 0
					  vec[0][0]=x location of handle 1									// This should be the actual value
					  vec[0][1]=y location of handle 1									// Unknown
					  vec[0][2]=z location of handle 1 (not used for FCurve Points(2d))	// This is zero
					  vec[1][0]=x location of control point								// Number of frame of the key
					  vec[1][1]=y location of control point								// Unknown
					  vec[1][2]=z location of control point								// This is zero
					  vec[2][0]=x location of handle 2									// Unknown
					  vec[2][1]=y location of handle 2									// Unknown
					  vec[2][2]=z location of handle 2 (not used for FCurve Points(2d))	// This is zero
					  // typedef struct BezTriple {
					  //	float vec[3][3];
					  //	float alfa, weight, radius;	// alfa: tilt in 3D View, weight: used for softbody goal weight, radius: for bevel tapering
					  //	short ipo;					// ipo: interpolation mode for segment from this BezTriple to the next
					  //	char h1, h2; 				// h1, h2: the handle type of the two handles
					  //	char f1, f2, f3;			// f1, f2, f3: used for selection status
					  //	char hide;					// hide: used to indicate whether BezTriple is hidden (3D), type of keyframe (eBezTriple_KeyframeTypes)
					  // } BezTriple;
				   */
		time = bt.vec[1][0];   //[2][0] or [3][0]
		value = bt.vec[2][1];
	}

	static void LoadAction(std::vector<BoneInfo>& boneInfos,
		std::map<std::string, uint32_t>& boneIndexMap,
		const Blender::bAction* act,
		float frs_sec = 24.f)
	{
		if (!act) return;
		std::string name = act->id.name;
		if (StartsWith(name, "AC")) name = name.substr(2);

		SPP_LOG(LOG_BLENDER, LOG_INFO, "LoadAction %s", name.c_str());

		if (name.size() > 4 && name.substr(name.size() - 4, 3) == ".00") return;   // Optional (We exclude actions that sometimes blender clones for apparently no reason)

		//if (mesh.getAnimationIndex(name) >= 0) return;

		std::vector< AnimationInfo > animationInfos;
		std::map< std::string, uint32_t > animationIndexMap;

		const unsigned index = animationInfos.size();

		//const Blender::bPoseChannel* c = (const Blender::bPoseChannel*) act->chanbase.first;
		//BlenderHelper::CycledCall<const Blender::bPoseChannel>(act->chanbase,&BlenderHelper::DisplayPoseChannelBriefly);
		//BlenderHelper::CycledCall<const Blender::bActionChannel>(act->chanbase,&BlenderHelper::Display);
		//BlenderHelper::CycledCall<const Blender::bActionGroup>(act->groups,&BlenderHelper::Display);
		//bPose has chanbase too

		bool animationOk = false;

		std::vector <BoneAnimationInfo::Vector3Key> translationKeys;
		std::vector <BoneAnimationInfo::QuaternionKey> rotationKeys;
		std::vector <BoneAnimationInfo::Vector3Key> scalingKeys;
		float animationTime = 0;    // number of key frames of the action

		BoneInfo* bi = NULL;
		bool newBone = false;
		EulerRotationMode eulerRotationMode = EulerRotationMode::EULER_XYZ;
		const Blender::FCurve* c = (const Blender::FCurve*)act->curves.first;
		while (c)
		{
			std::string rna = c->rna_path;
			//Now rna is something like: pose.bones["pelvis_body"].rotation_quaternion
			if (!StartsWith(rna, "pose.bones[\""))
			{
				c = c->next; continue;
			}
			rna = rna.substr(12);
			size_t beg = rna.find('\"');
			if (beg == std::string::npos) { c = c->next; continue; }
			const std::string boneName = rna.substr(0, beg);

			auto boneIdx = MapFindOrNull(boneIndexMap, std::string(boneName));
			BoneInfo* bi2 = boneIdx ? &boneInfos[*boneIdx] : nullptr;

			if (!bi2)
			{
				SPP_LOG(LOG_BLENDER, LOG_INFO, "Error: animation '%s' has a FCurve on a bone named: '%s', that does not exist in the (single) supported armature.", name.c_str(), boneName.c_str());
				{c = c->next; continue; }
			}

			if (bi != bi2)
			{
				bi = bi2;
				newBone = true;
				translationKeys.clear();
				rotationKeys.clear();
				scalingKeys.clear();
				translationKeys.reserve(c->totvert / 3); rotationKeys.reserve(c->totvert / 4); scalingKeys.reserve(c->totvert / 3);
			}
			else
			{
				newBone = false;
			}

			rna = rna.substr(beg + 1);
			beg = rna.find('.');
			if (beg == std::string::npos)
			{
				c = c->next; continue;
			}

			const std::string animationTypeString = rna.substr(beg + 1);

			enum class AnimationTypes : int8_t
			{
				Location = 0,
				Scaling = 1,
				Rotation_Euler = 2,
				Rotation_AxisAngle = 3,
				Rotation_Quat = 4,
				UNKNOWN = -1
			};

			// 0 = location, 1 = scaling, 2 = rotation_euler, 3 = rotation_axis_angle, 4 = rotation_quaternion, -1 = invalid
			const AnimationTypes animationTypeEnum =
				animationTypeString == "rotation_quaternion" ? AnimationTypes::Rotation_Quat :
				animationTypeString == "rotation_axis_angle" ? AnimationTypes::Rotation_AxisAngle :
				animationTypeString == "rotation_euler" ? AnimationTypes::Rotation_Euler :
				animationTypeString == "location" ? AnimationTypes::Location :
				(animationTypeString == "scaling" || animationTypeString == "scale") ? AnimationTypes::Scaling :
				AnimationTypes::UNKNOWN;

			if (animationTypeEnum == AnimationTypes::UNKNOWN)
			{
				c = c->next;
				SPP_LOG(LOG_BLENDER, LOG_INFO, "Error: can't decode animationTypeString = %s", animationTypeString.c_str());
				continue;
			}
			else if (animationTypeEnum == AnimationTypes::Rotation_Euler)
			{
				eulerRotationMode = (bi->eulerRotationMode > (EulerRotationMode)0 &&
					bi->eulerRotationMode < (EulerRotationMode)7) ?
					(EulerRotationMode)bi->eulerRotationMode : EulerRotationMode::EULER_XYZ;
			}

			const int animationTypeIndex = c->array_index;
			const bool isLastComponent = (animationTypeIndex == 2 && (int8_t)animationTypeEnum < 3) || (animationTypeIndex == 3 && (int8_t)animationTypeEnum >= 3);
			//const int numKeys = c->totvert; // Warning: it's the number of keys of EVERY single-value component!
			//printf("boneName: '%s' animationTypeString: '%s' animationTypeIndex: '%d' numKeys: %d\n",boneName.c_str(),animationTypeString.c_str(),animationTypeIndex,numKeys);

			//-----------------------------------

			if (c->bezt && c->totvert)
			{
				float time, value;
				BoneAnimationInfo::Vector3Key* lastTranslationKey = NULL;
				BoneAnimationInfo::Vector3Key* lastScalingKey = NULL;
				BoneAnimationInfo::QuaternionKey* lastQuaternionKey = NULL;

				int tki = 0, ski = 0, rki = 0;

				if (!newBone)
				{
					if (translationKeys.size() > 0) lastTranslationKey = &translationKeys[0];
					if (scalingKeys.size() > 0) lastScalingKey = &scalingKeys[0];
					if (rotationKeys.size() > 0) lastQuaternionKey = &rotationKeys[0];
				}

				for (int i = 0; i < c->totvert; i++)
				{
					GetTimeAndValue(c->bezt[i], time, value);
					//printf("\t%s: key[%d] = {%1.2f:  %1.4f};\n",bi->boneName.c_str(),i,time,value);

					switch (animationTypeEnum)
					{
					case AnimationTypes::Location: {
						if (animationTypeIndex == 0)
						{
							const size_t id = translationKeys.size();
							translationKeys.push_back(BoneAnimationInfo::Vector3Key());
							lastTranslationKey = &translationKeys[id];
							lastTranslationKey->time = time;
						}
						if (!lastTranslationKey)
						{
							fprintf(stderr, "An error has occurred while parsing an animation ('%s') with a 'loc' key for bone ('%s'): location[0] MUST be the first key in the .blend file.\n", name.c_str(), boneName.c_str());
							break;
						}
						BoneAnimationInfo::Vector3Key& loc = *lastTranslationKey;
						loc.value[animationTypeIndex] = value;
						if (isLastComponent)
						{
							if (animationTime < loc.time) animationTime = loc.time;
							BoneAnimationInfo* bai = bi->getBoneAnimationInfo(index);
							if (!bai)
							{
								animationOk = true;
								bai = &(bi->boneAnimationInfoMap[index] = BoneAnimationInfo());
								bai->reset();
							}
							bai->translationKeys.push_back(loc);
						}
						if (tki + 1 <= (int)translationKeys.size()) lastTranslationKey = &translationKeys[tki++];
					}
												 break;
					case AnimationTypes::Scaling: {
						if (animationTypeIndex == 0) {
							const size_t id = scalingKeys.size();
							scalingKeys.push_back(BoneAnimationInfo::Vector3Key());
							lastScalingKey = &scalingKeys[id];
							lastScalingKey->time = time;
						}
						if (!lastScalingKey) {
							fprintf(stderr, "An error has occurred while parsing an animation ('%s') with a 'sca' key for bone ('%s'): scaling[0] MUST be the first key in the .blend file.\n", name.c_str(), boneName.c_str());
							break;
						}
						BoneAnimationInfo::Vector3Key& sca = *lastScalingKey;
						sca.value[animationTypeIndex] = value;
						if (isLastComponent) {
							if (animationTime < sca.time) animationTime = sca.time;
							BoneAnimationInfo* bai = bi->getBoneAnimationInfo(index);
							if (!bai) {
								animationOk = true;
								bai = &(bi->boneAnimationInfoMap[index] = BoneAnimationInfo());
								bai->reset();
							}
							bai->scalingKeys.push_back(sca);
						}
						if (ski + 1 <= (int)scalingKeys.size()) lastScalingKey = &scalingKeys[ski++];
					}
												break;
					case AnimationTypes::Rotation_Euler:
					case AnimationTypes::Rotation_AxisAngle: {
						// rotation_euler (2) or rotation_axis_angle (3) [the latter to be tested]
						if (animationTypeIndex == 0) {
							const size_t id = rotationKeys.size();
							rotationKeys.push_back(BoneAnimationInfo::QuaternionKey());
							lastQuaternionKey = &rotationKeys[id];
							lastQuaternionKey->time = time;
						}
						if (!lastQuaternionKey) {
							fprintf(stderr, "An error has occurred while parsing an animation ('%s') with a 'rot' key for bone ('%s'): rotation_quaternion[0] MUST be the first key in the .blend file.\n", name.c_str(), boneName.c_str());
							break;
						}
						BoneAnimationInfo::QuaternionKey& qua = *lastQuaternionKey;
						qua.value.coeffs()[animationTypeIndex] = value;

						if (isLastComponent)
						{
							if (animationTypeEnum == AnimationTypes::Rotation_Euler)
							{
								const Vector3 eul(qua.value.coeffs()[0], qua.value.coeffs()[1], qua.value.coeffs()[2]);

								// Convert euler to quaternion: eul to qua-------------------------
								/*
								//const bool useAxisConventionOpenGL = true;
								static Matrix3 m;
								glm::mat3::FromEuler(m,eul[0],eul[1],eul[2],eulerRotationMode);//,useAxisConventionOpenGL);
								//fprintf(stderr,"From Euler animationTypeEnum=%d\n\n",animationTypeEnum);
								qua.value = glm::quat_cast(m);
								qua.value.x = -qua.value.x;qua.value.y = -qua.value.y;qua.value.z = -qua.value.z;qua.value.w = -qua.value.w;  // useless (but matches blender in my test-case
								qua.value.w = -qua.value.w;  //why???? Because glm::mat3::FromEuler(...) is wrong... in fact code below does not need it:
								*/

								const Quarternion qx(AxisAngle(eul(0), Vector3(1.0, 0.0, 0.0)));
								const Quarternion qy(AxisAngle(eul(1), Vector3(0.0, 1.0, 0.0)));
								const Quarternion qz(AxisAngle(eul(2), Vector3(0.0, 0.0, 1.0)));
								switch (eulerRotationMode)
								{
								case EulerRotationMode::EULER_XYZ: qua.value = qz * qy * qx; break;
								case EulerRotationMode::EULER_XZY: qua.value = qy * qz * qx; break;
								case EulerRotationMode::EULER_YXZ: qua.value = qz * qx * qy; break;
								case EulerRotationMode::EULER_YZX: qua.value = qx * qz * qy; break;
								case EulerRotationMode::EULER_ZXY: qua.value = qy * qx * qz; break;
								case EulerRotationMode::EULER_ZYX: qua.value = qx * qy * qz; break;
								default: qua.value = qz * qy * qx; break;
								}
								// is the code above wrapped in this line or not?
								//qua.value = glm::quat_FromEuler(eul[0],eul[1],eul[2],eulerRotationMode);
								qua.value.coeffs()(0) = -qua.value.coeffs()(0);
								qua.value.coeffs()(1) = -qua.value.coeffs()(1);
								qua.value.coeffs()(2) = -qua.value.coeffs()(2);
								qua.value.coeffs()(3) = -qua.value.coeffs()(3);

								/*static bool firstTime = true;if (firstTime) {firstTime=false;
									printf("%s:       {%1.4f   %1.4f   %1.4f}    =>  {%1.4f  %1.4f   %1.4f   %1.4f};\n",bi->boneName.c_str(),glm::degrees(eul[0]),glm::degrees(eul[1]),glm::degrees(eul[2]),qua.value.w,qua.value.x,qua.value.y,qua.value.z);
								}*/

							}
							else
							{
								// rotation_angle_axis NEVER TESTED
								const Vector3 axis(qua.value.coeffs()(0), qua.value.coeffs()(1), qua.value.coeffs()(2));
								const float angle = qua.value.coeffs()(3);

								qua.value = Quarternion(AxisAngle(angle, axis));
								//qua.value = -qua.value;  // useless (but matches blender in my test-case
								//qua.value.w = -qua.value.w;  //why????

								/*
								static bool firstTime = true;if (firstTime) {firstTime=false;
									printf("%s:       {%1.4f ; (%1.4f   %1.4f   %1.4f)}    =>  {%1.4f  %1.4f   %1.4f   %1.4f};\n",bi->boneName.c_str(),glm::degrees(angle),axis[0],axis[1],axis[2],qua.value.w,qua.value.x,qua.value.y,qua.value.z);
								}
								*/
							}
							//-----------------------------------------------------------------
							if (animationTime < qua.time) animationTime = qua.time;
							//glm::normalizeInPlace(qua.value);
							BoneAnimationInfo* bai = bi->getBoneAnimationInfo(index);
							if (!bai)
							{
								animationOk = true;
								bai = &(bi->boneAnimationInfoMap[index] = BoneAnimationInfo());
								bai->reset();
							}
							bai->rotationKeys.push_back(qua);
						}
						if (rki + 1 <= (int)rotationKeys.size()) lastQuaternionKey = &rotationKeys[rki++];
					}
														   break;
					case AnimationTypes::Rotation_Quat: {
						if (animationTypeIndex == 0) {
							const size_t id = rotationKeys.size();
							rotationKeys.push_back(BoneAnimationInfo::QuaternionKey());
							lastQuaternionKey = &rotationKeys[id];
							lastQuaternionKey->time = time;
						}
						if (!lastQuaternionKey) {
							fprintf(stderr, "An error has occurred while parsing an animation ('%s') with a 'rot' key for bone ('%s'): rotation_quaternion[0] MUST be the first key in the .blend file.\n", name.c_str(), boneName.c_str());
							break;
						}
						BoneAnimationInfo::QuaternionKey& qua = *lastQuaternionKey;
						//qua.value[animationTypeIndex] = value; // Nope, we need more clearness:
#define QUA_W_IS_FIRST_COMPONENT
#ifndef QUA_W_IS_FIRST_COMPONENT
						switch (animationTypeIndex) {
						case 0: qua.value.coeffs()(0) = value; break;
						case 1: qua.value.coeffs()(1) = value; break;
						case 2: qua.value.coeffs()(2) = value; break;
						case 3: qua.value.coeffs()(3) = value; break;
						default: break;
						}
#else //QUA_W_IS_FIRST_COMPONENT
						switch (animationTypeIndex) {
						case 0: qua.value.coeffs()(3) = value; break;
						case 1: qua.value.coeffs()(0) = value; break;
						case 2: qua.value.coeffs()(1) = value; break;
						case 3: qua.value.coeffs()(2) = value; break;
						default: break;
						}
#undef QUA_W_IS_FIRST_COMPONENT
#endif //QUA_W_IS_FIRST_COMPONENT
						if (isLastComponent)
						{
							if (animationTime < qua.time) animationTime = qua.time;
							//glm::normalizeInPlace(qua.value);
							BoneAnimationInfo* bai = bi->getBoneAnimationInfo(index);
							if (!bai) {
								animationOk = true;
								bai = &(bi->boneAnimationInfoMap[index] = BoneAnimationInfo());
								bai->reset();
							}
							bai->rotationKeys.push_back(qua);
						}
						if (rki + 1 <= (int)rotationKeys.size())
						{
							lastQuaternionKey = &rotationKeys[rki++];
						}
					}
													  break;
					default:
						break;
					}

				}
			}

			//-----------------------------------

			c = c->next;
		}

		// second pass:-----------------------------------------
		const bool displayDebug = true;
		const bool mergeFrameKeys = false;  // The worst framekey decimator I've ever seen...
		const float mergeTimeEps = 3.01f;   // Twickable
		size_t maxNumFrames = 0;
		const Vector3 zeroVector3(0, 0, 0);
		const Vector3 oneVector3(1, 1, 1);
		const Quarternion zeroQuat(1, 0, 0, 0);
		for (size_t i = 0, sz = boneInfos.size(); i < sz; i++)
		{
			BoneInfo& bi = boneInfos[i];
			BoneAnimationInfo* pbai = bi.getBoneAnimationInfo(index);
			if (!pbai) continue;
			BoneAnimationInfo& bai = *pbai;

			// Testing only (to remove)
			bai.postState = BoneAnimationInfo::AB_REPEAT;

			// Setting bai.tkAndRkHaveUniqueTimePerKey and similiar---------
			const unsigned numPositonKeys = bai.translationKeys.size();
			const unsigned numRotationKeys = bai.rotationKeys.size();
			const unsigned numScalingKeys = bai.scalingKeys.size();
			const unsigned numKeys = numPositonKeys > 0 ? numPositonKeys : numRotationKeys > 0 ? numRotationKeys : numScalingKeys > 0 ? numScalingKeys : 0;
			if (numKeys != 0) {
				bool ok = true &&
					(numPositonKeys == 0 || numKeys == numPositonKeys) &&
					(numRotationKeys == 0 || numKeys == numRotationKeys) &&
					(numScalingKeys == 0 || numKeys == numScalingKeys);
				if (ok) {
					if (numPositonKeys > 0) {
						if (numRotationKeys > 0) {
							// Check equality P=R
							for (unsigned k = 0; k < numKeys; k++) {
								if (bai.translationKeys[k].time != bai.rotationKeys[k].time) { ok = false; break; }
							}
							if (ok) bai.tkAndRkHaveUniqueTimePerKey = true;
							ok = true;
						}
						if (numScalingKeys > 0) {
							// Check equality P=S
							for (unsigned k = 0; k < numKeys; k++) {
								if (bai.translationKeys[k].time != bai.scalingKeys[k].time) { ok = false; break; }
							}
							if (ok) bai.tkAndSkHaveUniqueTimePerKey = true;
							ok = true;
						}
					}
					if (numRotationKeys > 0) {
						if (numScalingKeys > 0) {
							// Check equality R=S
							for (unsigned k = 0; k < numKeys; k++) {
								if (bai.rotationKeys[k].time != bai.scalingKeys[k].time) { ok = false; break; }
							}
							if (ok) bai.rkAndSkHaveUniqueTimePerKey = true;
							ok = true;
						}
					}
				}
			}

			maxNumFrames = numPositonKeys;
			if (numRotationKeys > 0 && (!bai.tkAndRkHaveUniqueTimePerKey || maxNumFrames == 0)) {
				maxNumFrames += numRotationKeys;   // wrong
				if (numScalingKeys > 0 && !bai.tkAndSkHaveUniqueTimePerKey && !bai.rkAndSkHaveUniqueTimePerKey)  maxNumFrames += numScalingKeys;   // wrong
			}
			else if (numScalingKeys > 0 && ((!bai.tkAndSkHaveUniqueTimePerKey || (numRotationKeys > 0 && !bai.rkAndSkHaveUniqueTimePerKey)) || maxNumFrames == 0))
				maxNumFrames += numScalingKeys;   // wrong

			if (displayDebug)
			{
				printf("bone: '%s':\n", bi.boneName.c_str());
				for (size_t j = 0; j < bai.translationKeys.size(); j++)
				{
					const BoneAnimationInfo::Vector3Key& k = bai.translationKeys[j];
					printf("\ttra[%d: %1.2f] = {%1.4f %1.4f %1.4f};\n", (int)j, k.time, k.value[0], k.value[1], k.value[2]);
				}
				for (size_t j = 0; j < bai.scalingKeys.size(); j++)
				{
					const BoneAnimationInfo::Vector3Key& k = bai.scalingKeys[j];
					printf("\tsca[%d: %1.2f] = {%1.4f %1.4f %1.4f};\n", (int)j, k.time, k.value[0], k.value[1], k.value[2]);
				}
				for (size_t j = 0; j < bai.rotationKeys.size(); j++)
				{
					const BoneAnimationInfo::QuaternionKey& k = bai.rotationKeys[j];
					printf("\tqua[%d: %1.2f] = {%1.4f %1.4f %1.4f,%1.4f};\n", (int)j, k.time, k.value.coeffs()(3), k.value.coeffs()(0), k.value.coeffs()(1), k.value.coeffs()(2));
				}
			}
		}
		//------------------------------------------------------


		// Setting global Mesh::AnimationInfo
		if (animationOk)
		{
			animationInfos.push_back(AnimationInfo());
			animationIndexMap[name] = index;
			AnimationInfo& ai = animationInfos[index];

			ai.name = name;

			//fbtPrintf("animationName:'%s' animationTime:%1.4f maxNumFrames:%d frs_sec:%1.4f\n",name.c_str(),animationTime,(int)maxNumFrames,frs_sec);       
			ai.numTicks = animationTime;
			ai.ticksPerSecond = frs_sec > 0 ? frs_sec : 24;//maxNumFrames > 0 ? (float)maxNumFrames : 24.0;
			ai.duration = ai.numTicks / ai.ticksPerSecond;    // mmmh: this should be animationTime!!!
		}
	}

	static void LoadAnimations(std::vector<BoneInfo>& boneInfos,
		std::map<std::string, uint32_t>& boneIndexMap,
		const Blender::AnimData* a, float frs_sec = 24.f)
	{
		if (!a) return;
		const Blender::bAction* action = a->action;
		if (action)
		{
			LoadAction(boneInfos, boneIndexMap, action, frs_sec);    // Just 1 action ?
		}
	}


	bool LoadBlenderFile(const AssetPath& FileName, LoadedMeshes& oMeshes)
	{
		SPP_LOG(LOG_BLENDER, LOG_INFO, "Loading Asset: %s", *FileName);

		fbtBlend blendfile; // we keep it scoped inside InitGL() to save memory
		if (blendfile.parse(*FileName) != fbtFile::FS_OK)
		{
			SPP_LOG(LOG_BLENDER, LOG_INFO, "FAILED BLENDER LOAD: %s", *FileName);
			return false;
		}

		std::shared_ptr<Armature> foundArmature;

		fbtList& objects = blendfile.m_object; long cnt = -1;
		for (Blender::Object* ob = (Blender::Object*)objects.first; ob; ob = (Blender::Object*)ob->id.next)
		{
			++cnt;
						
			SPP_LOG(LOG_BLENDER, LOG_INFO, "OBJECT %d", ob->type);

			if (!ob->data)	continue;

			if (ob->type == BL_OBTYPE_ARMATURE)
			{
#if 0
				if (!foundArmature)
				{
					foundArmature = std::make_shared<Armature>();
					LoadArmature(ob, foundArmature->boneInfos, foundArmature->boneIndexMap, foundArmature->numValidBones, foundArmature->rootBoneInfos);
				}

				////////////////////////////////////////////////////////////////////////
				//LOAD ANIMATIONS
				////////////////////////////////////////////////////////////////////////
				if (ob->adt)
				{
					float frs_sec = 24;
					LoadAnimations(foundArmature->boneInfos, foundArmature->boneIndexMap, (const Blender::AnimData*)ob->adt, frs_sec);
				}
#endif
				continue;
			}
			if (ob->type != BL_OBTYPE_MESH)
			{
				continue;
			}

			const Blender::Mesh* me = (const Blender::Mesh*)ob->data;
			// Warning: when we use ALT+D in Blender to duplicate/link a mesh, we still
			// find here the same "me" we have used before.
			// But also note that linked copies can have different materials (see below in the MATERIALS section).

			// VALIDATION-------------------------------------------
			SPP_LOG(LOG_BLENDER, LOG_INFO, " - mesh found : %s", me->id.name);

			const bool hasFaces = me->totface > 0 && me->mface;
			const bool hasPolys = me->totpoly > 0 && me->mpoly && me->totloop > 0 && me->mloop;
			const bool hasVerts = me->totvert > 0;
			const bool isValid = hasVerts && (hasFaces || hasPolys);
			if (!isValid) continue;

			SPP_LOG(LOG_BLENDER, LOG_INFO, " - verts : %d", me->totvert);
			SPP_LOG(LOG_BLENDER, LOG_INFO, " - faces : %d", me->totface);
			SPP_LOG(LOG_BLENDER, LOG_INFO, " - polys : %d", me->totpoly);
			SPP_LOG(LOG_BLENDER, LOG_INFO, " - colors : %d", me->totcol);

			const Blender::ModifierData* mdf = (const Blender::ModifierData*)ob->modifiers.first;
			const Blender::bArmature* currentMeshArmature = nullptr;
			const Blender::Object* currentMeshArmatureObj = nullptr;

			while (mdf)
			{
				SPP_LOG(LOG_BLENDER, LOG_INFO, " - has modifier '%s' had modifier: %s (type=%d mode=%dd)", ob->id.name, mdf->name, mdf->type, mdf->mode);


				if (mdf->type == 8)
				{
					const Blender::ArmatureModifierData* md = (const Blender::ArmatureModifierData*)mdf;					
					currentMeshArmature = BlenderHelper::GetArmature(md->object);
					if (currentMeshArmature == nullptr)
					{
						SPP_LOG(LOG_BLENDER, LOG_INFO, "Error: object %s has an armature modifier without name.", ob->id.name);
					}
					else
					{
						currentMeshArmatureObj = md->object;
					}
				}

				mdf = mdf->next;    // see next modifier
			}

			auto& layer = oMeshes.Layers.emplace_back(LoadedMeshes::MeshLayer{ std::make_shared<ArrayResource >(),std::make_shared <ArrayResource >() });
			float mMatrix[16]; // The one stored in ob->obmat, in float16 format

			// Filling mMatrix
			{
				// Retrieve data from ob:
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						mMatrix[j * 4 + i] = ob->obmat[j][i];
					}
				}
			}

			// VERTS AND NORMS--(NOT MIRRORED)--------------------------
			int numVerts = me->totvert;
			auto pvertices = layer.VertexResource->InitializeFromType< MeshVertex>(numVerts);

			constexpr float MAX_SHORT = static_cast<float>(std::numeric_limits<int16_t>::max());
			for (int i = 0; i < numVerts; i++)
			{
				const Blender::MVert& v = me->mvert[i];

				MeshVertex& vertex = pvertices[i];
				vertex.position = BlenderHelper::ToVector3(v.co);
				float corNorm[] = { v.no[0] / MAX_SHORT, v.no[1] / MAX_SHORT, v.no[2] / MAX_SHORT };
				vertex.normal = BlenderHelper::ToVector3(corNorm);
			}

			std::vector<uint32_t> indices;
			if (hasFaces)
			{
				bool isTriangle = me->mface[0].v4 == 0 ? true : false;	// Here I suppose that 0 can't be a vertex index in the last point of a quad face (e.g. quad(0,1,2,0) is a triangle(0,1,2)). This assunction SHOULD BE WRONG, but I don't know how to spot if a face is a triangle or a quad...
				
				for (int i = 0; i < me->totface; i++)
				{
					const Blender::MFace& mface = me->mface[i];
					
					isTriangle = (mface.v4 == 0);
					const Blender::MTFace* mtf = &me->mtface[i];

					// Add (first) triangle:
					indices.push_back(mface.v1);
					indices.push_back(mface.v2);
					indices.push_back(mface.v3);
					
					if (!isTriangle)
					{
						// Add second triangle:
						indices.push_back(mface.v3);
						indices.push_back(mface.v4);
						indices.push_back(mface.v1);						
					}
				}
			}

			//}
			if (hasPolys)
			{
				
				for (int i = 0; i < me->totpoly; i++)
				{
					const Blender::MPoly& poly = me->mpoly[i];					
					const int numLoops = poly.totloop;
					
					//MLoopUV* mloopuv;
					//MLoopCol* mloopcol;

					{
						// Is triangle or quad:
						if (numLoops == 3 || numLoops == 4)
						{
							// draw first triangle
							if (poly.loopstart + 2 >= me->totloop)
							{
								SPP_LOG(LOG_BLENDER, LOG_INFO, "me->mtpoly ERROR: poly.loopstart+2(%d)>=me->totloop(%d)", poly.loopstart + 2, me->totloop);
								continue;
							}

							auto li0 = poly.loopstart;
							auto li1 = poly.loopstart + 1;
							auto li2 = poly.loopstart + 2;

							auto vi0 = me->mloop[li0].v;
							auto vi1 = me->mloop[li1].v;
							auto vi2 = me->mloop[li2].v;

							if (vi0 >= me->totvert || vi1 >= me->totvert || vi2 >= me->totvert) continue;

							// Add (first) triangle:
							indices.push_back(vi0);
							indices.push_back(vi1);
							indices.push_back(vi2);						

							// isQuad
							if (numLoops == 4)
							{
								if (poly.loopstart + 3 >= me->totloop)
								{
									SPP_LOG(LOG_BLENDER, LOG_INFO, "me->mtpoly ERROR: poly.loopstart+3(%d)>=me->totloop(%d)", poly.loopstart + 3, me->totloop);
									continue;
								}
								auto li3 = poly.loopstart + 3;
								auto vi3 = me->mloop[li3].v; if (vi3 >= me->totvert) continue;

								// Add second triangle ----------------
								indices.push_back(vi2);
								indices.push_back(vi3);
								indices.push_back(vi0);								
								//--------------------------------------
							}


						}
						else if (numLoops > 4)
						{
							//is n-gon
							//--------------------------------------------------------------------
							const int faceSize = poly.totloop;
							int currentFirstVertPos = 0;
							int currentLastVertPos = faceSize - 1;
							int remainingFaceSize = faceSize;
							bool isQuad = true;

							do
							{
								isQuad = remainingFaceSize >= 4;

								if (poly.loopstart + currentLastVertPos >= me->totloop)
								{
									SPP_LOG(LOG_BLENDER, LOG_INFO, "me->mtpoly ERROR: poly.loopstart + currentLastVertPos(%d)>=me->totloop(%d)", poly.loopstart + currentLastVertPos, me->totloop);
									break;
								}

								auto li0 = poly.loopstart + currentLastVertPos;
								auto li1 = poly.loopstart + currentFirstVertPos;
								auto li2 = poly.loopstart + currentFirstVertPos + 1;

								auto vi0 = me->mloop[li0].v;
								auto vi1 = me->mloop[li1].v;
								auto vi2 = me->mloop[li2].v;

								if (vi0 >= me->totvert || vi1 >= me->totvert || vi2 >= me->totvert) break;

								// Add triangle -----------------------------------------------------
								indices.push_back(vi0);
								indices.push_back(vi1);
								indices.push_back(vi2);
								// -----------------------------------------------------------------------

								if (isQuad) 
								{
									if (poly.loopstart + currentLastVertPos >= me->totloop) {
										SPP_LOG(LOG_BLENDER, LOG_INFO, "me->mtpoly ERROR: poly.loopstart + currentLastVertPos(%d)>=me->totloop(%d)", poly.loopstart + currentLastVertPos, me->totloop);
										break;
									}

									li0 = poly.loopstart + currentFirstVertPos + 1;
									li1 = poly.loopstart + currentLastVertPos - 1;
									li2 = poly.loopstart + currentLastVertPos;

									vi0 = me->mloop[li0].v;
									vi1 = me->mloop[li1].v;
									vi2 = me->mloop[li2].v;

									if (vi0 >= me->totvert || vi1 >= me->totvert || vi2 >= me->totvert) break;

									// Add triangle -----------------------------------------------------
									indices.push_back(vi0);
									indices.push_back(vi1);
									indices.push_back(vi2);									
									// -----------------------------------------------------------------------
								}

								++currentFirstVertPos;
								--currentLastVertPos;
								remainingFaceSize -= 2;

							} while (remainingFaceSize >= 3);
							//-------------------------------------------------------------------------------------------
						}
					}
				}
			}

			auto pIndices = layer.IndexResource->InitializeFromType<uint32_t>(indices.size());
			memcpy(pIndices.GetData(), indices.data(), layer.IndexResource->GetTotalSize());

			////////////////////////////////////////////////////////////////////////
			//LOAD BONES & WEIGHTS
			////////////////////////////////////////////////////////////////////////

#if 0
			if (currentMeshArmatureObj && !foundArmature)
			{
				foundArmature = std::make_shared<Armature>();
				LoadArmature(ob, foundArmature->boneInfos, foundArmature->boneIndexMap, foundArmature->numValidBones, foundArmature->rootBoneInfos);
			}

			auto numSingleVerts = numVerts;
			std::string boneParentName;
			if (strlen(ob->parsubstr) > 0) boneParentName = ob->parsubstr;

			// VERTEX DEFORM GROUP NAMES----------------------------------------
			if (boneParentName.size() == 0)
			{
				std::vector<std::string> deformGroupNames;        // They should match the bone names
				std::map<int, int> deformGroupBoneIdMap;
				const Blender::bDeformGroup* dg = (const Blender::bDeformGroup*)ob->defbase.first;
				while (dg)
				{
					const int dgId = deformGroupNames.size();
					deformGroupNames.push_back(dg->name);
					//printf("%d) %s\n",deformGroupNames.size()-1,deformGroupNames[deformGroupNames.size()-1].c_str());
					if (foundArmature->boneInfos.size() > 0)
					{
						auto boneIdx = MapFindOrNull(foundArmature->boneIndexMap, std::string(dg->name));
						const BoneInfo* boneInfo = boneIdx ? &foundArmature->boneInfos[*boneIdx] : nullptr;
						if (boneInfo)
						{
							deformGroupBoneIdMap[dgId] = (int)boneInfo->index;
							//printf("deformGroupBoneIdMap[%d] = %d\n",dgId,(int) boneInfo->index);   // This line is good for debugging
						}
						else
						{
							deformGroupBoneIdMap[dgId] = -1;
							fprintf(stderr, "Error: Deform group: '%s' has no matching bone inside armature\n", dg->name);
						}
					}
					dg = dg->next;
				}

				// VERTS DEFORM DATA (TODO: Mirror these when the mirror modifier is present)-
				if (me->dvert)
				{
					//BlenderHelper::DisplayDeformGroups(me);
					std::shared_ptr<BoneWeightData> boneWeightData = std::make_shared< BoneWeightData>();

					std::vector < BoneIdsType >& boneIndices = boneWeightData->boneIndices;
					std::vector < BoneWeightsType >& boneWeights = boneWeightData->boneWeights;

					boneIndices.resize(numSingleVerts);
					boneWeights.resize(numSingleVerts);
					//const int numSingleVerts = me->totvert;
					for (int i = 0; i < numSingleVerts; i++)
					{
						const Blender::MDeformVert& d = me->dvert[i];
						BlenderHelper::FillDeformGroups(d, boneIndices[i], boneWeights[i], &deformGroupBoneIdMap);
					}
				}
			}
			else if (foundArmature->boneInfos.size() > 0)
			{
				auto boneIdx = MapFindOrNull(foundArmature->boneIndexMap, boneParentName);
				const BoneInfo* boneInfo = boneIdx ? &foundArmature->boneInfos[*boneIdx] : nullptr;
				if (boneInfo)
				{
					// Fill weights:
					std::shared_ptr<BoneWeightData> boneWeightData = std::make_shared< BoneWeightData>();

					std::vector < BoneIdsType > &boneIndices = boneWeightData->boneIndices;
					std::vector < BoneWeightsType > &boneWeights = boneWeightData->boneWeights;
					boneIndices.resize(numSingleVerts);
					boneWeights.resize(numSingleVerts);
					//const int numSingleVerts = me->totvert;
					for (int i = 0; i < numSingleVerts; i++) {
						BoneIdsType& bi = boneIndices[i];
						BoneWeightsType& bw = boneWeights[i];
						bi[0] = boneInfo->index;    // (unsigned) parentBoneId;
						bw[0] = 1.f;
						for (int j = 1; j < 4; j++) { bi[j] = 0; bw[j] = 0.f; }
					}
				}
				else
				{
					SPP_LOG(LOG_BLENDER, LOG_INFO, "Error: the mesh '%s' has a parent bone named: '%s', that does not exist inside current armature (we support a single armature).", 
						ob->id.name, boneParentName.c_str());
				}
			}
#endif
		}

		return true;
	}
}