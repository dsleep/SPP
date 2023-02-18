// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPSceneO.h"
#include "SPPGraphicsO.h"
#include "SPPPhysX.h"
#include <string>

#if _WIN32 && !defined(SPP_GAMEENGINE_STATIC)

	#ifdef SPP_GAMEENGINE_EXPORT
		#define SPP_GAMEENGINE_API __declspec(dllexport)
	#else
		#define SPP_GAMEENGINE_API __declspec(dllimport)
	#endif

	#else

		#define SPP_GAMEENGINE_API 

#endif



namespace SPP
{
	class SPP_GAMEENGINE_API VgEntity : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND
			
	protected:
		VgEntity(const std::string& InName, SPPDirectory* InParent);
		
		bool _bIsActive = true;

	public:
		virtual void Update(float DeltaTime) {};

		virtual ~VgEntity() { }
	};

	class SPP_GAMEENGINE_API VgEnvironment : public ORenderableScene
	{
		RTTR_ENABLE(ORenderableScene);
		RTTR_REGISTRATION_FRIEND

	protected:
		std::shared_ptr< PhysicsScene > _physicsScene;

		VgEnvironment(const std::string& InName, SPPDirectory* InParent);

		std::list< VgEntity* > _entities;

	public:
		PhysicsScene* GetPhysicsScene()
		{
			return _physicsScene.get();
		}

		//std::list< VgEntity* >& GetEntities()
		//{
		//	return _entities;
		//}

		virtual void Update(float DeltaTime);

		virtual ~VgEnvironment() { }
	};

	class SPP_GAMEENGINE_API VgCapsuleElement : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		std::shared_ptr< PhysicsCharacter > _physicsPrimitive;
		float _height = 1.5f;
		float _radius = 0.75f;

		VgCapsuleElement(const std::string& InName, SPPDirectory* InParent) : OElement(InName, InParent) { }

	public:
		virtual ~VgCapsuleElement() { }

		virtual void Move(const Vector3d& InDelta, float DeltaTime);

		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene() override;
	};

	//Vg video game? do i like this hmmmmm

	class SPP_GAMEENGINE_API VgMeshElement : public OMeshElement
	{
		RTTR_ENABLE(OMeshElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		std::shared_ptr< PhysicsPrimitive > _physicsPrimitive;

		VgMeshElement(const std::string& InName, SPPDirectory* InParent) : OMeshElement(InName, InParent) { }
		
	public:
		virtual ~VgMeshElement() { }

		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene() override;
	};


	class SPP_GAMEENGINE_API VgRig : public VgEntity
	{
		RTTR_ENABLE(VgEntity);
		RTTR_REGISTRATION_FRIEND

	protected:
		VgRig(const std::string& InName, SPPDirectory* InParent);

	public:
		virtual void Update(float DeltaTime);
		virtual ~VgRig() { }
	};

	class SPP_GAMEENGINE_API VgSVVO : public VgEntity
	{
		RTTR_ENABLE(VgEntity);
		RTTR_REGISTRATION_FRIEND

	protected:
		VgSVVO(const std::string& InName, SPPDirectory* InParent);

		float _voxelSize = 1.0f;
		std::unique_ptr< class SparseVirtualizedVoxelOctree > _SVVO;
		std::shared_ptr<RT_RenderableSVVO> _renderableSVVO;

	public:
		virtual ~VgSVVO() { }

		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene() override;
	};


	SPP_GAMEENGINE_API uint32_t GetGameEngineVersion();
}

