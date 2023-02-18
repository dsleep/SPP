// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGameEngine.h"
#include "SPPFileSystem.h"
#include "SPPMath.h"

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
	
	void VgSVVO::AddedToScene(class OScene* InScene)
	{
		VgEntity::AddedToScene(InScene);

		_SVVO = std::make_unique< SparseVirtualizedVoxelOctree>(_translation, _scale, _voxelSize, 65536 );

		if (!InScene) return;

		auto thisRenderableScene = dynamic_cast<ORenderableScene*>(InScene);
		SE_ASSERT(thisRenderableScene);

		// not ready yet
		if (!thisRenderableScene->GetGraphicsDevice()
			|| !thisRenderableScene->GetRenderScene()) return;

		auto sceneGD = thisRenderableScene->GetGraphicsDevice();
		_renderableSVVO = sceneGD->CreateRenderableSVVO();
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