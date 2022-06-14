// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPPhysX.h"

#include "PxPhysicsAPI.h"
#include "SPPPlatformCore.h"
#include "SPPMemory.h"
#include "SPPHandledTimers.h"

#include "characterkinematic/PxBoxController.h"
#include "characterkinematic/PxCapsuleController.h"
#include "characterkinematic/PxControllerManager.h"

#include <mutex>
#include <condition_variable>
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


	struct StepperCallbacks
	{
		std::function<void(float)> onSubstepStart;
		std::function<void(void)> onSubstepPreFetchResult;
		std::function<void(float)> onSubstep;
		std::function<void(float, PxBaseTask*)> onSubstepSetup;
	};

	////////////////////
	class Stepper
	{
	public:
		Stepper() {}
		virtual					~Stepper() {}

		virtual	bool			advance(PxScene* scene, PxReal dt, void* scratchBlock, PxU32 scratchBlockSize) = 0;
		virtual	void			wait(PxScene* scene) = 0;
		virtual void			substepStrategy(const PxReal stepSize, PxU32& substepCount, PxReal& substepSize) = 0;
		virtual void			postRender(const PxReal stepSize) = 0;

		virtual void			setSubStepper(const PxReal stepSize, const PxU32 maxSteps) {}
		virtual	void			renderDone() {}
		virtual	void			shutdown() {}

		PxReal					getSimulationTime()	const { return mSimulationTime; }
		StepperCallbacks&		getCallbacks() { return _callbacks; }
	protected:
		StepperCallbacks		_callbacks;
		PxReal					mSimulationTime;
	};

	class MultiThreadStepper;
	class StepperTask : public physx::PxLightCpuTask
	{
	public:
		void setStepper(MultiThreadStepper* stepper) { mStepper = stepper; }
		MultiThreadStepper* getStepper() { return mStepper; }
		const MultiThreadStepper* getStepper() const { return mStepper; }
		const char* getName() const { return "Stepper Task"; }
		void run();
	protected:
		MultiThreadStepper* mStepper;
	};

	class StepperTaskSimulate : public StepperTask
	{

	public:
		StepperTaskSimulate() {}
		void run();

	};
	
	

	class MultiThreadStepper : public Stepper
	{
	public:
		MultiThreadStepper()
			: mFirstCompletionPending(false)
			, mScene(NULL)
			, mCurrentSubStep(0)
			, mNbSubSteps(0)
		{
			mCompletion0.setStepper(this);
			mCompletion1.setStepper(this);
			mSimulateTask.setStepper(this);
		}

		~MultiThreadStepper() {}

		virtual bool advance(PxScene* scene, PxReal dt, void* scratchBlock, PxU32 scratchBlockSize)
		{
			mScratchBlock = scratchBlock;
			mScratchBlockSize = scratchBlockSize;

			substepStrategy(dt, mNbSubSteps, mSubStepSize);

			if (mNbSubSteps == 0) return false;

			mScene = scene;

			ready = false;

			mCurrentSubStep = 1;

			mCompletion0.setContinuation(*mScene->getTaskManager(), NULL);

			mSimulationTime = 0.0f;
			mTimer.getElapsedSeconds();

			// take first substep
			substep(mCompletion0);
			mFirstCompletionPending = true;

			return true;
		}
		virtual void substepDone(StepperTask* ownerTask)
		{
			if(_callbacks.onSubstepPreFetchResult)
			_callbacks.onSubstepPreFetchResult();

			{
#if !PX_PROFILE
				PxSceneWriteLock writeLock(*mScene);
#endif
				mScene->fetchResults(true);
			}

			PxReal delta = (PxReal)mTimer.getElapsedSeconds();
			mSimulationTime += delta;

			if(_callbacks.onSubstep)
			_callbacks.onSubstep(mSubStepSize);

			if (mCurrentSubStep >= mNbSubSteps)
			{
				{
					std::lock_guard lk(m);
					ready = true;
				}
				cv.notify_one();
			}
			else
			{
				StepperTask& s = ownerTask == &mCompletion0 ? mCompletion1 : mCompletion0;
				s.setContinuation(*mScene->getTaskManager(), NULL);
				mCurrentSubStep++;

				mTimer.getElapsedSeconds();

				substep(s);

				// after the first substep, completions run freely
				s.removeReference();
			}
		}
		virtual void renderDone()
		{
			if (mFirstCompletionPending)
			{
				mCompletion0.removeReference();
				mFirstCompletionPending = false;
			}
		}

		virtual void postRender(const PxReal stepSize) {}

		// if mNbSubSteps is 0 then the sync will never 
		// be set so waiting would cause a deadlock
		virtual void wait(PxScene* scene) 
		{
			if (mNbSubSteps)
			{
				std::unique_lock lk(m);
				cv.wait(lk, [&] {return ready; });
			}
		}
		virtual void shutdown() { }
		virtual void reset() = 0;
		virtual void substepStrategy(const PxReal stepSize, PxU32& substepCount, PxReal& substepSize) = 0;
		virtual void simulate(physx::PxBaseTask* ownerTask)
		{
			PxSceneWriteLock writeLock(*mScene);
			mScene->simulate(mSubStepSize, ownerTask, mScratchBlock, mScratchBlockSize);
		}
		PxReal getSubStepSize() const { return mSubStepSize; }

	protected:
		void substep(StepperTask& completionTask)
		{
			// setup any tasks that should run in parallel to simulate()
			if(_callbacks.onSubstepSetup)
			_callbacks.onSubstepSetup(mSubStepSize, &completionTask);

			// step
			{
				mSimulateTask.setContinuation(&completionTask);
				mSimulateTask.removeReference();
			}
			// parallel sample tasks are started in mSolveTask (after solve was called which acquires a write lock).
		}

		// we need two completion tasks because when multistepping we can't submit completion0 from the
		// substepDone function which is running inside completion0
		bool				mFirstCompletionPending;
		StepperTaskSimulate	mSimulateTask;
		StepperTask			mCompletion0, mCompletion1;
		PxScene* mScene;

		STDElapsedTimer		mTimer;

		PxU32				mCurrentSubStep;
		PxU32				mNbSubSteps;
		PxReal				mSubStepSize;
		void* mScratchBlock;
		PxU32				mScratchBlockSize;


		bool ready = false;
		std::mutex m;
		std::condition_variable cv;
	};

	void StepperTask::run()
	{
		mStepper->substepDone(this);
		release();
	}

	void StepperTaskSimulate::run()
	{
		mStepper->simulate(mCont);
		auto& callbacks = mStepper->getCallbacks();
		if(callbacks.onSubstepStart)
			callbacks.onSubstepStart(mStepper->getSubStepSize());
	}

	class DebugStepper : public Stepper
	{
	public:
		DebugStepper(const PxReal stepSize) : mStepSize(stepSize) {}

		virtual void substepStrategy(const PxReal stepSize, PxU32& substepCount, PxReal& substepSize)
		{
			substepCount = 1;
			substepSize = mStepSize;
		}

		virtual bool advance(PxScene* scene, PxReal dt, void* scratchBlock, PxU32 scratchBlockSize);

		virtual void postRender(const PxReal stepSize)
		{
		}

		virtual void setSubStepper(const PxReal stepSize, const PxU32 maxSteps)
		{
			mStepSize = stepSize;
		}

		virtual void wait(PxScene* scene);

		PxReal mStepSize;
	};

	// The way this should be called is:
	// bool stepped = advance(dt)
	//
	// ... reads from the scene graph for rendering
	//
	// if(stepped) renderDone()
	//
	// ... anything that doesn't need access to the physics scene
	//
	// if(stepped) sFixedStepper.wait()
	//
	// Note that per-substep callbacks to the sample need to be issued out of here, 
	// between fetchResults and simulate

	class FixedStepper : public MultiThreadStepper
	{
	public:
		FixedStepper(const PxReal subStepSize, const PxU32 maxSubSteps)
			: MultiThreadStepper()
			, mAccumulator(0)
			, mFixedSubStepSize(subStepSize)
			, mMaxSubSteps(maxSubSteps)
		{
		}

		virtual void substepStrategy(const PxReal stepSize, PxU32& substepCount, PxReal& substepSize)
		{
			if (mAccumulator > mFixedSubStepSize)
				mAccumulator = 0.0f;

			// don't step less than the step size, just accumulate
			mAccumulator += stepSize;
			if (mAccumulator < mFixedSubStepSize)
			{
				substepCount = 0;
				return;
			}

			substepSize = mFixedSubStepSize;
			substepCount = PxMin(PxU32(mAccumulator / mFixedSubStepSize), mMaxSubSteps);

			mAccumulator -= PxReal(substepCount) * substepSize;
		}
		virtual void reset() { mAccumulator = 0.0f; }

		virtual void setSubStepper(const PxReal stepSize, const PxU32 maxSteps) { mFixedSubStepSize = stepSize; mMaxSubSteps = maxSteps; }

		virtual void postRender(const PxReal stepSize)
		{
		}

		PxReal	mAccumulator;
		PxReal	mFixedSubStepSize;
		PxU32	mMaxSubSteps;
	};


	class VariableStepper : public MultiThreadStepper
	{
	public:
		VariableStepper(const PxReal minSubStepSize, const PxReal maxSubStepSize, const PxU32 maxSubSteps)
			: MultiThreadStepper()
			, mAccumulator(0)
			, mMinSubStepSize(minSubStepSize)
			, mMaxSubStepSize(maxSubStepSize)
			, mMaxSubSteps(maxSubSteps)
		{
		}

		virtual void substepStrategy(const PxReal stepSize, PxU32& substepCount, PxReal& substepSize)
		{
			if (mAccumulator > mMaxSubStepSize)
				mAccumulator = 0.0f;

			// don't step less than the min step size, just accumulate
			mAccumulator += stepSize;
			if (mAccumulator < mMinSubStepSize)
			{
				substepCount = 0;
				return;
			}

			substepCount = PxMin(PxU32(PxCeil(mAccumulator / mMaxSubStepSize)), mMaxSubSteps);
			substepSize = PxMin(mAccumulator / substepCount, mMaxSubStepSize);

			mAccumulator -= PxReal(substepCount) * substepSize;
		}
		virtual void	reset() { mAccumulator = 0.0f; }

	private:
		VariableStepper& operator=(const VariableStepper&);
		PxReal	mAccumulator;
		const	PxReal	mMinSubStepSize;
		const	PxReal	mMaxSubStepSize;
		const	PxU32	mMaxSubSteps;
	};

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
		PhysXTriangleMesh(PxTriangleMesh* InMesh, const void* InData, uint32_t DataSize) : _triMesh(InMesh)
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
		const void* indices,
		uint32_t indexStride,
		const MeshCreationSettings& meshSettings)
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

	class PhysXCharacter : public PhysicsCharacter
	{
	protected:
		PxController* _pxController = nullptr;
		PxRigidActor* _pxActor = nullptr;

	public:
		PhysXCharacter(PxController* InController) : _pxController(InController)
		{
			_pxActor = _pxController->getActor();
		}

		virtual ~PhysXCharacter()
		{
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
		PxScene* _scene = nullptr;
		PxControllerManager* _controllerManager = nullptr;
		VariableStepper _stepper;

		float _simulationTime = 0.0f;

		static const PxU32 SCRATCH_BLOCK_SIZE = 1024 * 128;
		std::vector<uint8_t> _scratchBlock;

	public:
		PhysXScene() : _stepper(1.0f / 80.0f, 1.0f / 40.0f, 8)
		{
			_scratchBlock.resize(SCRATCH_BLOCK_SIZE);

			PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
			sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
			sceneDesc.cpuDispatcher = gDispatcher;
			sceneDesc.filterShader = PxDefaultSimulationFilterShader;
			_scene = gPhysics->createScene(sceneDesc);

			_controllerManager = PxCreateControllerManager(*_scene);

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
			OElement* InElement,
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
			newPxActor->userData = InElement;
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

		virtual std::shared_ptr< PhysicsCharacter > CreateCharacterCapsule(const Vector3& Extents,
			OElement* InElement) override
		{
			PxCapsuleControllerDesc capsuleDesc;

			float radius = Extents[0];
			float height = Extents[1];

			capsuleDesc.height = height;
			capsuleDesc.radius = radius;
			capsuleDesc.climbingMode = PxCapsuleClimbingMode::eEASY;

			#define CONTACT_OFFSET			0.1f
			#define STEP_OFFSET				0.1f
			#define SLOPE_LIMIT				0.0f
			#define INVISIBLE_WALLS_HEIGHT	0.0f
			#define MAX_JUMP_HEIGHT			0.0f
			
			capsuleDesc.slopeLimit = SLOPE_LIMIT;
			capsuleDesc.contactOffset = CONTACT_OFFSET;
			capsuleDesc.stepOffset = STEP_OFFSET;
			capsuleDesc.invisibleWallHeight = INVISIBLE_WALLS_HEIGHT;
			capsuleDesc.maxJumpHeight = MAX_JUMP_HEIGHT;

			auto &elePos = InElement->GetPosition();
			capsuleDesc.position.x = elePos[0];
			capsuleDesc.position.y = elePos[1];
			capsuleDesc.position.z = elePos[2];

			//capsuleDesc.density = desc.mProxyDensity;
			//capsuleDesc.scaleCoeff = desc.mProxyScale;
			//capsuleDesc.material = &mOwner.getDefaultMaterial();
			//capsuleDesc.position = desc.mPosition;
			//capsuleDesc.slopeLimit = desc.mSlopeLimit;
			//capsuleDesc.contactOffset = desc.mContactOffset;
			//capsuleDesc.stepOffset = desc.mStepOffset;
			//capsuleDesc.invisibleWallHeight = desc.mInvisibleWallHeight;
			//capsuleDesc.maxJumpHeight = desc.mMaxJumpHeight;
			////	capsuleDesc.nonWalkableMode		= PxControllerNonWalkableMode::ePREVENT_CLIMBING_AND_FORCE_SLIDING;
			//capsuleDesc.reportCallback = desc.mReportCallback;
			//capsuleDesc.behaviorCallback = desc.mBehaviorCallback;
			//capsuleDesc.volumeGrowth = desc.mVolumeGrowth;
			//
			//capsuleDesc.userData = InElement;

			PxController* ctrl = _controllerManager->createController(capsuleDesc);
			ctrl->getActor()->userData = InElement;

			return std::make_shared< PhysXCharacter >(ctrl);
		}

		virtual void Update(float DeltaTime) override
		{
			// in profile builds we run the whole frame sequentially
			// simulate, wait, update render objects, render			
			auto waitForResults = _stepper.advance(_scene, DeltaTime, _scratchBlock.data(), _scratchBlock.size());
			_stepper.renderDone();
			if (waitForResults)
			{
				_stepper.wait(_scene);
				_simulationTime = _stepper.getSimulationTime();
			
				UpdateTransforms();
			}
		}

		void UpdateTransforms()
		{
			PxSceneReadLock scopedLock(*_scene);

			uint32_t activeTransformCount = 0;
			PxActor** activeTransforms = _scene->getActiveActors(activeTransformCount);

			for (uint32_t Iter = 0; Iter < activeTransformCount; Iter++)
			{
				PxActor* actor = activeTransforms[Iter];
				const PxType actorType = actor->getConcreteType();
				if (actorType == PxConcreteType::eRIGID_DYNAMIC || 
					actorType == PxConcreteType::eRIGID_STATIC || 
					actorType == PxConcreteType::eARTICULATION_LINK || 
					actorType == PxConcreteType::eARTICULATION_JOINT)
				{
					PxRigidActor* rigidActor = static_cast<PxRigidActor*>(actor);

					auto globalPose = rigidActor->getGlobalPose();

					auto curElement = reinterpret_cast<OElement*>(rigidActor->userData);

					//PxU32 nbShapes = rigidActor->getNbShapes();
					//for (PxU32 i = 0; i < nbShapes; i++)
					//{
					//	PxShape* shape;
					//	PxU32 n = rigidActor->getShapes(&shape, 1, i);
					//	PX_ASSERT(n == 1);
					//	PX_UNUSED(n);
					//	
					//	//
					//	//rigidActor->userData
					//	//shape->userData
					//}
				}
			}
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
			const void* indices,
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

	PhysicsAPI* GetPhysicsAPI()
	{
		static PhysXAPI sO;
		return &sO;
	}
}