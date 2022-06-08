// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPPhysX.h"

#include "PxPhysicsAPI.h"
#include "SPPPlatformCore.h"

#include <chrono>

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
	PxCooking* gCooking = NULL;

	PxPvd* gPvd = NULL;
	
	#define PVD_HOST "127.0.0.1"

	// Setup common cooking params
	void setupCommonCookingParams(PxCookingParams& params, bool skipMeshCleanup, bool skipEdgeData)
	{
		// we suppress the triangle mesh remap table computation to gain some speed, as we will not need it 
		// in this snippet
		params.suppressTriangleMeshRemapTable = true;

		// If DISABLE_CLEAN_MESH is set, the mesh is not cleaned during the cooking. The input mesh must be valid. 
		// The following conditions are true for a valid triangle mesh :
		//  1. There are no duplicate vertices(within specified vertexWeldTolerance.See PxCookingParams::meshWeldTolerance)
		//  2. There are no large triangles(within specified PxTolerancesScale.)
		// It is recommended to run a separate validation check in debug/checked builds, see below.

		if (!skipMeshCleanup)
			params.meshPreprocessParams &= ~static_cast<PxMeshPreprocessingFlags>(PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH);
		else
			params.meshPreprocessParams |= PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;

		// If DISABLE_ACTIVE_EDGES_PREDOCOMPUTE is set, the cooking does not compute the active (convex) edges, and instead 
		// marks all edges as active. This makes cooking faster but can slow down contact generation. This flag may change 
		// the collision behavior, as all edges of the triangle mesh will now be considered active.
		if (!skipEdgeData)
			params.meshPreprocessParams &= ~static_cast<PxMeshPreprocessingFlags>(PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE);
		else
			params.meshPreprocessParams |= PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE;
	}

	struct MeshCreationSettings
	{
		bool skipMeshCleanup = false;
		bool skipEdgeData = false;
		bool inserted = false;
		bool cookingPerformance = false;
		bool meshSizePerfTradeoff = false;
	};

	void createTriangleMesh(PxU32 numVertices, 
		const PxVec3* vertices,
		PxU32 numTriangles, 
		const PxU32* indices,
		const MeshCreationSettings &meshSettings)
	{
		auto startTime = std::chrono::steady_clock::now();

		PxTriangleMeshDesc meshDesc;
		meshDesc.points.count = numVertices;
		meshDesc.points.data = vertices;
		meshDesc.points.stride = sizeof(PxVec3);
		meshDesc.triangles.count = numTriangles;
		meshDesc.triangles.data = indices;
		meshDesc.triangles.stride = 3 * sizeof(PxU32);

		PxCookingParams params = gCooking->getParams();

		// setup common cooking params
		setupCommonCookingParams(params, meshSettings.skipMeshCleanup, meshSettings.skipEdgeData);

		// Create BVH33 midphase
		params.midphaseDesc = PxMeshMidPhase::eBVH33;

		// The COOKING_PERFORMANCE flag for BVH33 midphase enables a fast cooking path at the expense of somewhat lower quality BVH construction.	
		if (meshSettings.cookingPerformance)
			params.midphaseDesc.mBVH33Desc.meshCookingHint = PxMeshCookingHint::eCOOKING_PERFORMANCE;
		else
			params.midphaseDesc.mBVH33Desc.meshCookingHint = PxMeshCookingHint::eSIM_PERFORMANCE;

		// If meshSizePerfTradeoff is set to true, smaller mesh cooked mesh is produced. The mesh size/performance trade-off
		// is controlled by setting the meshSizePerformanceTradeOff from 0.0f (smaller mesh) to 1.0f (larger mesh).
		if (meshSettings.meshSizePerfTradeoff)
		{
			params.midphaseDesc.mBVH33Desc.meshSizePerformanceTradeOff = 0.0f;
		}
		else
		{
			// using the default value
			params.midphaseDesc.mBVH33Desc.meshSizePerformanceTradeOff = 0.55f;
		}

		gCooking->setParams(params);

#if defined(PX_CHECKED) || defined(PX_DEBUG)
		// If DISABLE_CLEAN_MESH is set, the mesh is not cleaned during the cooking. 
		// We should check the validity of provided triangles in debug/checked builds though.
		if (meshSettings.skipMeshCleanup)
		{
			PX_ASSERT(gCooking->validateTriangleMesh(meshDesc));
		}
#endif // DEBUG


		PxTriangleMesh* triMesh = NULL;
		PxU32 meshSize = 0;

		// The cooked mesh may either be saved to a stream for later loading, or inserted directly into PxPhysics.
		if (meshSettings.inserted)
		{
			triMesh = gCooking->createTriangleMesh(meshDesc, gPhysics->getPhysicsInsertionCallback());
		}
		else
		{
			PxDefaultMemoryOutputStream outBuffer;
			gCooking->cookTriangleMesh(meshDesc, outBuffer);

			PxDefaultMemoryInputData stream(outBuffer.getData(), outBuffer.getSize());
			triMesh = gPhysics->createTriangleMesh(stream);

			meshSize = outBuffer.getSize();
		}

		// Print the elapsed time for comparison		
		auto elapsedTime = (int32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
		printf("\t -----------------------------------------------\n");
		printf("\t Create triangle mesh with %d triangles: \n", numTriangles);
		meshSettings.cookingPerformance ? printf("\t\t Cooking performance on\n") : printf("\t\t Cooking performance off\n");
		meshSettings.inserted ? printf("\t\t Mesh inserted on\n") : printf("\t\t Mesh inserted off\n");
		!meshSettings.skipEdgeData ? printf("\t\t Precompute edge data on\n") : printf("\t\t Precompute edge data off\n");
		!meshSettings.skipMeshCleanup ? printf("\t\t Mesh cleanup on\n") : printf("\t\t Mesh cleanup off\n");
		printf("\t\t Mesh size/performance trade-off: %f \n", double(params.midphaseDesc.mBVH33Desc.meshSizePerformanceTradeOff));
		printf("\t Elapsed time in ms: %d \n", elapsedTime);
		if (!meshSettings.inserted)
		{
			printf("\t Mesh size: %d \n", meshSize);
		}

		triMesh->release();
	}

//	// Creates a triangle mesh using BVH34 midphase with different settings.
//	void createBV34TriangleMesh(PxU32 numVertices, const PxVec3* vertices, PxU32 numTriangles, const PxU32* indices,
//		bool skipMeshCleanup, bool skipEdgeData, bool inserted, const PxU32 numTrisPerLeaf)
//	{
//		PxU64 startTime = SnippetUtils::getCurrentTimeCounterValue();
//
//		PxTriangleMeshDesc meshDesc;
//		meshDesc.points.count = numVertices;
//		meshDesc.points.data = vertices;
//		meshDesc.points.stride = sizeof(PxVec3);
//		meshDesc.triangles.count = numTriangles;
//		meshDesc.triangles.data = indices;
//		meshDesc.triangles.stride = 3 * sizeof(PxU32);
//
//		PxCookingParams params = gCooking->getParams();
//
//		// Create BVH34 midphase
//		params.midphaseDesc = PxMeshMidPhase::eBVH34;
//
//		// setup common cooking params
//		setupCommonCookingParams(params, skipMeshCleanup, skipEdgeData);
//
//		// Cooking mesh with less triangles per leaf produces larger meshes with better runtime performance
//		// and worse cooking performance. Cooking time is better when more triangles per leaf are used.
//		params.midphaseDesc.mBVH34Desc.numPrimsPerLeaf = numTrisPerLeaf;
//
//		gCooking->setParams(params);
//
//#if defined(PX_CHECKED) || defined(PX_DEBUG)
//		// If DISABLE_CLEAN_MESH is set, the mesh is not cleaned during the cooking. 
//		// We should check the validity of provided triangles in debug/checked builds though.
//		if (skipMeshCleanup)
//		{
//			PX_ASSERT(gCooking->validateTriangleMesh(meshDesc));
//		}
//#endif // DEBUG
//
//
//		PxTriangleMesh* triMesh = NULL;
//		PxU32 meshSize = 0;
//
//		// The cooked mesh may either be saved to a stream for later loading, or inserted directly into PxPhysics.
//		if (inserted)
//		{
//			triMesh = gCooking->createTriangleMesh(meshDesc, gPhysics->getPhysicsInsertionCallback());
//		}
//		else
//		{
//			PxDefaultMemoryOutputStream outBuffer;
//			gCooking->cookTriangleMesh(meshDesc, outBuffer);
//
//			PxDefaultMemoryInputData stream(outBuffer.getData(), outBuffer.getSize());
//			triMesh = gPhysics->createTriangleMesh(stream);
//
//			meshSize = outBuffer.getSize();
//		}
//
//		// Print the elapsed time for comparison
//		PxU64 stopTime = SnippetUtils::getCurrentTimeCounterValue();
//		float elapsedTime = SnippetUtils::getElapsedTimeInMilliseconds(stopTime - startTime);
//		printf("\t -----------------------------------------------\n");
//		printf("\t Create triangle mesh with %d triangles: \n", numTriangles);
//		inserted ? printf("\t\t Mesh inserted on\n") : printf("\t\t Mesh inserted off\n");
//		!skipEdgeData ? printf("\t\t Precompute edge data on\n") : printf("\t\t Precompute edge data off\n");
//		!skipMeshCleanup ? printf("\t\t Mesh cleanup on\n") : printf("\t\t Mesh cleanup off\n");
//		printf("\t\t Num triangles per leaf: %d \n", numTrisPerLeaf);
//		printf("\t Elapsed time in ms: %f \n", double(elapsedTime));
//		if (!inserted)
//		{
//			printf("\t Mesh size: %d \n", meshSize);
//		}
//
//		triMesh->release();
//	}



	//class Jump
	//{
	//public:
	//	Jump();

	//	PxF32		mV0;
	//	PxF32		mJumpTime;
	//	bool		mJump;

	//	void		startJump(PxF32 v0);
	//	void		stopJump();
	//	PxF32		getHeight(PxF32 elapsedTime);
	//};


	//static PxF32 gJumpGravity = -50.0f;

	//Jump::Jump() :
	//	mV0(0.0f),
	//	mJumpTime(0.0f),
	//	mJump(false)
	//{
	//}

	//void Jump::startJump(PxF32 v0)
	//{
	//	if (mJump)	return;
	//	mJumpTime = 0.0f;
	//	mV0 = v0;
	//	mJump = true;
	//}

	//void Jump::stopJump()
	//{
	//	if (!mJump)	return;
	//	mJump = false;
	//	//mJumpTime = 0.0f;
	//	//mV0	= 0.0f;
	//}

	//PxF32 Jump::getHeight(PxF32 elapsedTime)
	//{
	//	if (!mJump)	return 0.0f;
	//	mJumpTime += elapsedTime;
	//	const PxF32 h = gJumpGravity * mJumpTime * mJumpTime + mV0 * mJumpTime;
	//	return h * elapsedTime;
	//}



	//class ControlledActor 
	//{
	//protected:
	//	PhysXSample& mOwner;
	//	PxControllerShapeType::Enum	mType;
	//	Jump						mJump;

	//	PxExtendedVec3				mInitialPosition;
	//	PxVec3						mDelta;
	//	bool						mTransferMomentum;

	//	PxController* mController;
	//	PxReal						mStandingSize;
	//	PxReal						mCrouchingSize;
	//	PxReal						mControllerRadius;
	//	bool						mDoStandup;
	//	bool						mIsCrouching;

	//public:
	//	ControlledActor(PhysXSample& owner);
	//	virtual										~ControlledActor();

	//	PxController* init(const ControlledActorDesc& desc, PxControllerManager* manager);
	//	PxExtendedVec3				getFootPosition()	const;
	//	void						reset();
	//	void						teleport(const PxVec3& pos);
	//	void						sync();
	//	void						tryStandup();
	//	void						resizeController(PxReal height);
	//	void						resizeStanding() { resizeController(mStandingSize); }
	//	void						resizeCrouching() { resizeController(mCrouchingSize); }
	//	void						jump(float force) { mJump.startJump(force); }

	//	PX_FORCE_INLINE	PxController* getController() { return mController; }

	//	const Jump& getJump() const { return mJump; }
	//};

	void InitializePhysX()
	{
#if _DEBUG
		AddDLLSearchPath("../3rdParty/PhysX1.4/PhysX/bin/win.x86_64.vc143.md/debug");
#else
		AddDLLSearchPath("../3rdParty/PhysX1.4/PhysX/bin/win.x86_64.vc143.md/release");
#endif

		gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);
		SE_ASSERT(gFoundation);
		gPvd = PxCreatePvd(*gFoundation);
		SE_ASSERT(gPvd);
		PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
		gPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);
		gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true, gPvd);
		SE_ASSERT(gPhysics);
		gCooking = PxCreateCooking(PX_PHYSICS_VERSION, *gFoundation, PxCookingParams(PxTolerancesScale()));
	}

	void ShutdownPhysX()
	{
		gPhysics->release();
		gCooking->release();
		gFoundation->release();
	}
}