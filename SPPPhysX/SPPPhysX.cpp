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
	PxDefaultCpuDispatcher* gDispatcher = NULL;

	PxMaterial* gMaterial = NULL;

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

	class PhysXTriangleMesh : public PhysicsTriangleMesh
	{
	protected:
		PxTriangleMesh* _triMesh = nullptr;
		std::vector<uint8_t> _buffer;

	public:
		PhysXTriangleMesh(PxTriangleMesh* InMesh, const void *InData, uint32_t DataSize) : _triMesh(InMesh) 
		{
			_buffer.resize(DataSize);
			memcpy(_buffer.data(), InData, DataSize);
		}

		virtual DataView GetData() override
		{
			return { _buffer.data(), _buffer.size() };
		}
		
		virtual ~PhysXTriangleMesh()
		{
			_triMesh->release();
		}
		PxTriangleMesh* GetTriMesh()
		{
			return _triMesh;
		}
	};

	std::shared_ptr<PhysXTriangleMesh> createTriangleMesh(uint32_t numVertices,
		const void* vertData,
		uint32_t stride,
		uint32_t numTriangles,
		const uint32_t* indices,
		uint32_t indexStride,
		const MeshCreationSettings &meshSettings)
	{
		auto startTime = std::chrono::steady_clock::now();

		PxTriangleMeshDesc meshDesc;
		meshDesc.points.count = numVertices;
		meshDesc.points.data = vertData;
		meshDesc.points.stride = stride;
		meshDesc.triangles.count = numTriangles;
		meshDesc.triangles.data = indices;
		meshDesc.triangles.stride = indexStride;

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
		PxDefaultMemoryOutputStream outBuffer;

		// The cooked mesh may either be saved to a stream for later loading, or inserted directly into PxPhysics.
		{			
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

		return std::make_shared< PhysXTriangleMesh >(triMesh, outBuffer.getData(), outBuffer.getSize());
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
		gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.6f);
		gDispatcher = PxDefaultCpuDispatcherCreate(2);
	}

	void ShutdownPhysX()
	{
		gPhysics->release();
		gCooking->release();
		gFoundation->release();
	}

	

	class PhysXPrimitive : public PhysicsPrimitive
	{
	protected:
		PxRigidActor* _pxActor = nullptr;

	public:
		PhysXPrimitive(PxRigidActor* InActor) : _pxActor(InActor)
		{

		}

		virtual ~PhysXPrimitive() 
		{
		}

		virtual bool IsDynamic() override
		{
			return false;// _pxActor->
		}

		virtual Vector3d GetPosition() override
		{
			auto globalPose = _pxActor->getGlobalPose();
			return Vector3d(globalPose.p.x, globalPose.p.y, globalPose.p.z);
		}
		virtual Vector3 GetRotation() override
		{
			const float RadToDegree = 57.295755f;
			auto globalPose = _pxActor->getGlobalPose();
			Eigen::Quaternion<float> q(&globalPose.q.x);
			auto euler = q.toRotationMatrix().eulerAngles(0, 1, 2);
			return Vector3(euler[0] * RadToDegree, euler[1] * RadToDegree, euler[2] * RadToDegree);
		}
		virtual Vector3 GetScale() override
		{
			return Vector3(1, 1, 1);
		}

		virtual void SetPosition(const Vector3d& InValue) override
		{

		}
		virtual void SetRotation(const Vector3& InValue) override
		{

		}
		virtual void SetScale(const Vector3& InValue) override
		{

		}
	};

	class PhysXScene : public PhysicsScene
	{
	protected:
		PxScene* _scene = NULL;

	public:
		PhysXScene()
		{
			PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
			sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
			sceneDesc.cpuDispatcher = gDispatcher;
			sceneDesc.filterShader = PxDefaultSimulationFilterShader;
			_scene = gPhysics->createScene(sceneDesc);

#if _DEBUG
			PxPvdSceneClient* pvdClient = _scene->getScenePvdClient();
			if (pvdClient)
			{
				pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
				pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
				pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
			}
#endif
		}

		virtual ~PhysXScene()
		{
			_scene->release();
		}

		Eigen::Quaternion<float> ToQuaterionFromEuler(const Vector3& InRotationEulerYPRDegrees)
		{
			const float degToRad = 0.0174533f;
			Eigen::AngleAxisf yawAngle(InRotationEulerYPRDegrees[0] * degToRad, Vector3::UnitY());
			Eigen::AngleAxisf pitchAngle(InRotationEulerYPRDegrees[1] * degToRad, Vector3::UnitX());
			Eigen::AngleAxisf rollAngle(InRotationEulerYPRDegrees[2] * degToRad, Vector3::UnitZ());
			return Eigen::Quaternion<float>(rollAngle * yawAngle * pitchAngle);
		}

		PxTransform ToPxTransform(const Vector3d& InPosition, const Vector3& InRotationEulerYPRDegrees)
		{
			auto q = ToQuaterionFromEuler(InRotationEulerYPRDegrees);
			return PxTransform(PxVec3(InPosition[0], InPosition[1], InPosition[2]), PxQuat(q.coeffs()[0], q.coeffs()[1], q.coeffs()[2], q.coeffs()[3]));
		}		

		virtual std::shared_ptr< PhysicsPrimitive > CreateBoxPrimitive(const Vector3d& InPosition,
			const Vector3& InRotationEuler,
			const Vector3& Extents,
			bool bIsDynamic = false) override
		{
			auto actorTransform = ToPxTransform(InPosition, InRotationEuler);

			PxBoxGeometry Geom(Extents[0], Extents[1], Extents[2]);
			PxRigidActor* newPxActor = nullptr;
			if (bIsDynamic)
			{
				newPxActor = PxCreateDynamic(*gPhysics, actorTransform, Geom, *gMaterial, 1.0f);
			}
			else
			{
				newPxActor = PxCreateStatic(*gPhysics, actorTransform, Geom, *gMaterial);
			}
			_scene->addActor(*newPxActor);

			return std::make_shared< PhysXPrimitive >(newPxActor);
		}

		virtual std::shared_ptr< PhysicsPrimitive > CreateTriangleMeshPrimitive(const Vector3d& InPosition,
			const Vector3& InRotationEuler,
			const Vector3& InScale,
			std::shared_ptr< PhysicsTriangleMesh > InTriMesh) override
		{
			SE_ASSERT(InTriMesh);

			auto actorTransform = ToPxTransform(InPosition, InRotationEuler);

			PxTriangleMeshGeometry Geom;

			auto triMeshCast = std::dynamic_pointer_cast<PhysXTriangleMesh>(InTriMesh);
			Geom.triangleMesh = triMeshCast->GetTriMesh();

			PxRigidActor* newPxActor = PxCreateStatic(*gPhysics, actorTransform, Geom, *gMaterial);
			_scene->addActor(*newPxActor);

			return std::make_shared< PhysXPrimitive >(newPxActor);
		}
	};


	class PhysXAPI : public PhysicsAPI
	{
	protected:

	public:
		virtual std::shared_ptr< PhysicsScene > CreatePhysicsScene() override
		{
			return std::make_shared< PhysXScene >();
		}

		virtual std::shared_ptr< PhysicsTriangleMesh > CreateTriangleMesh(uint32_t numVertices,
			const void* vertData,
			uint32_t stride,
			uint32_t numTriangles,
			const uint32_t* indices,
			uint32_t indexStride) override
		{
			return createTriangleMesh(numVertices,
				vertData,
				stride,
				numTriangles,
				indices,
				indexStride,
				MeshCreationSettings{});
		}
	};
}