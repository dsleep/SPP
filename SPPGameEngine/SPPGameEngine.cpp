// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGameEngine.h"
#include "SPPFileSystem.h"
#include "SPPMath.h"
#include "SPPSparseVirtualizedVoxelOctree.h"

#include <utility>
SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	VgEntity::VgEntity(const std::string& InName, SPPDirectory* InParent) : OElement(InName, InParent)
	{
	}
	VgEnvironment::VgEnvironment(const std::string& InName, SPPDirectory* InParent) : ORenderableScene(InName, InParent)
	{
		_physicsScene = GetPhysicsAPI()->CreatePhysicsScene();
	}

	void VgEnvironment::Update(float DeltaTime)
	{
		std::list< VgEntity* > copyEntities = _entities;

		for (auto& curEntity : copyEntities)
		{
			if (curEntity) //&& !curEntity->IsPendingGC()
			{
				curEntity->Update(DeltaTime);
			}
		}

		_physicsScene->Update(DeltaTime);
	}

	void VgMeshElement::AddedToScene(class OScene* InScene)
	{
		OMeshElement::AddedToScene(InScene);
#if 0
		auto thisEnvironment = dynamic_cast<VgEnvironment*>(InScene);
		SE_ASSERT(thisEnvironment);

		if (_meshObj && _meshObj->GetMesh())
		{
			auto thisMesh = _meshObj->GetMesh();
			auto &curMeshes = thisMesh->GetMeshElements();

			if (!curMeshes.empty())
			{
				auto vertices = curMeshes.front()->VertexResource;
				auto indices = curMeshes.front()->IndexResource;
				
				SE_ASSERT(vertices->GetPerElementSize() == sizeof(MeshVertex));
				SE_ASSERT(indices->GetPerElementSize() == sizeof(uint32_t));

				auto meshVerts = vertices->GetSpan<MeshVertex>();

				auto triMesh = GetPhysicsAPI()->CreateTriangleMesh(vertices->GetElementCount(), &meshVerts[0].position, sizeof(MeshVertex), 
					indices->GetElementCount() / 3, indices->GetElementData(), sizeof(uint32_t) * 3);

				auto currentScene = thisEnvironment->GetPhysicsScene();


				_physicsPrimitive = currentScene->CreateTriangleMeshPrimitive(this->_translation, this->_rotation, this->_scale, triMesh);
			}
		}
#endif
	}

	void VgMeshElement::RemovedFromScene()
	{
		OMeshElement::RemovedFromScene();
		_physicsPrimitive.reset();
	}

	void VgCapsuleElement::AddedToScene(class OScene* InScene)
	{
		OElement::AddedToScene(InScene);

		auto thisEnvironment = dynamic_cast<VgEnvironment*>(InScene);
		SE_ASSERT(thisEnvironment);

		auto currentScene = thisEnvironment->GetPhysicsScene();

		_physicsPrimitive = currentScene->CreateCharacterCapsule(Vector3(_radius, _height, _radius), this);
	}
	void VgCapsuleElement::RemovedFromScene()
	{
		OElement::RemovedFromScene();
		_physicsPrimitive.reset();
	}
	void VgCapsuleElement::Move(const Vector3d & InDelta, float DeltaTime)
	{
		_physicsPrimitive->Move(InDelta, DeltaTime);
		_translation = _physicsPrimitive->GetPosition();
	}

	//
	VgRig::VgRig(const std::string& InName, SPPDirectory* InParent) : VgEntity(InName, InParent)
	{
	}

	void VgRig::Update(float DeltaTime)
	{

	}

	VgSVVO::VgSVVO(const std::string& InName, SPPDirectory* InParent) : VgEntity(InName, InParent)
	{
	}

	SparseVirtualizedVoxelOctree* VgSVVO::GetSVVO()
	{
		return _SVVO.get();
	}

	void VgSVVO::SendUpdates(UpdateMsg* InMsgs, uint32_t InMsgCount)
	{

	}
	
	void VgSVVO::AddedToScene(class OScene* InScene)
	{
		VgEntity::AddedToScene(InScene);

		_SVVO = std::make_unique<SparseVirtualizedVoxelOctree>(_translation, _scale, _voxelSize, 65536 );

		if (!InScene) return;

		auto thisRenderableScene = dynamic_cast<ORenderableScene*>(InScene);
		SE_ASSERT(thisRenderableScene);

		// not ready yet
		if (!thisRenderableScene->GetGraphicsDevice()
			|| !thisRenderableScene->GetRenderScene()) return;

		auto sceneGD = thisRenderableScene->GetGraphicsDevice();
		_renderableSVVO = sceneGD->CreateRenderableSVVO();

		_bounds = Sphere(_translation, _scale.maxCoeff());

		_renderableSVVO->SetArgs({
			.position = _translation,
			.eulerRotationYPR = _rotation,
			.scale = _scale,
			.bIsStatic = _bIsStatic,
			.bounds = _bounds
			});

		_renderableSVVO->AddToRenderScene(thisRenderableScene->GetRenderScene());

		auto levelCount = _SVVO->GetLevelCount();

		for (uint32_t Iter = 0; Iter < levelCount; Iter++)
		{
			auto &curBuffer = _renderableSVVO->GetBufferLevel(Iter);
			curBuffer = sceneGD->CreateBuffer(
				_SVVO->IsLevelVirtual(Iter) ? GPUBufferType::Sparse : GPUBufferType::Array);
		}

	}

	void VgSVVO::FullRTUpdate()
	{
		auto TotalActivePages = _SVVO->GetActivePageCount();
		auto pageSize = _SVVO->GetPageSize();

		std::vector<uint32_t> LevelSizes;
		std::vector<uint32_t> LevelPagesSizes;
		std::vector<BufferPageData> bufferData[MAX_VOXEL_LEVELS];

		auto levelCount = _SVVO->GetLevelCount();

		for (uint32_t Iter = 0; Iter < levelCount; Iter++)
		{
			LevelSizes.push_back(_SVVO->GetLevelMaxSize(Iter));
			LevelPagesSizes.push_back(_SVVO->GetLevelPageSize(Iter));
		}

		uint32_t curoffset = 0;
		auto memData = std::make_shared< std::vector<uint8_t> >();
		memData->resize(TotalActivePages * pageSize);
		auto TotalSize = memData->size();
		// backup pages
		_SVVO->TouchAllActivePages([&, directData = memData->data()](uint8_t InLevel, uint32_t InPage, const void* InMem) {
			SE_ASSERT(curoffset < TotalSize);

			auto currentPageSize = LevelPagesSizes[InLevel];
			memcpy(directData + curoffset, InMem, currentPageSize);
			bufferData[InLevel].push_back({ directData + curoffset, InPage});
			curoffset += currentPageSize;
		});
		
		RunOnRT([_renderableSVVO = this->_renderableSVVO, bufferData, LevelSizes, memData]() mutable
			{
				auto directData = memData->data();
				for (int32_t Iter = 0; Iter < LevelSizes.size(); Iter++)
				{
					auto& thisStoredLevel = bufferData[Iter];
					auto& thisBufLevel = _renderableSVVO->GetBufferLevel(Iter);
					
					std::shared_ptr< ArrayResource > newData = std::make_shared< ArrayResource >();
					newData->InitializeFromType(

					thisBufLevel->Initialize(LevelSizes[Iter]);
					
					if (thisStoredLevel.size())
					{
						thisBufLevel->GetGPUBuffer()->SetSparsePageMem(thisStoredLevel.data(), thisStoredLevel.size());
					}					
				}
			});
	}

	void VgSVVO::RemovedFromScene()
	{
		VgEntity::RemovedFromScene();

		if (_renderableSVVO)
		{
			RunOnRT([_renderableSVVO = this->_renderableSVVO]()
			{
				_renderableSVVO->RemoveFromRenderScene();
			});
			_renderableSVVO.reset();
		}
	}

	uint32_t GetGameEngineVersion()
	{
		return 1;
	}
}


using namespace SPP;

RTTR_REGISTRATION
{
	rttr::registration::class_<VgEntity>("VgEntity")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<VgEnvironment>("VgEnvironment")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_entities", &VgEnvironment::_entities)(rttr::policy::prop::as_reference_wrapper)
		;	

	rttr::registration::class_<VgMeshElement>("VgMeshElement")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		;

	rttr::registration::class_<VgCapsuleElement>("VgCapsuleElement")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		;

	rttr::registration::class_<VgRig>("VgRig")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		;

	rttr::registration::class_<VgSVVO>("VgSVVO")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		;	
}