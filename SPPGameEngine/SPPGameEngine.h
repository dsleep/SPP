// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPSceneO.h"
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
	class SPP_GAMEENGINE_API OEntity : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND
			
	protected:
		OEntity(const std::string& InName, SPPDirectory* InParent);
		
		bool _bIsActive = true;

	public:
		virtual void Update(float DeltaTime) {};
	};

	class SPP_GAMEENGINE_API OEnvironment : public OScene
	{
		RTTR_ENABLE(OScene);
		RTTR_REGISTRATION_FRIEND

	protected:
		OEnvironment(const std::string& InName, SPPDirectory* InParent);

		std::list< OEntity* > _entities;

	public:
		virtual void Update(float DeltaTime);
	};

	SPP_GAMEENGINE_API uint32_t GetGameEngineVersion();
}

