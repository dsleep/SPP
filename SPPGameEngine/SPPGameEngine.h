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

		virtual void Update(float DeltaTime);

		virtual ~VgEnvironment() { }
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

	SPP_GAMEENGINE_API uint32_t GetGameEngineVersion();
}

