// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPObject.h"
#include "SPPMath.h"

#if _WIN32 && !defined(SPP_SCENE_STATIC)

	#ifdef SPP_SCENE_EXPORT
		#define SPP_SCENE_API __declspec(dllexport)
	#else
		#define SPP_SCENE_API __declspec(dllimport)
	#endif

	#else

		#define SPP_SCENE_API 

#endif


namespace SPP
{
	class SPP_SCENE_API OElement : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OElement(const MetaPath& InPath) : SPPObject(InPath) { }

		Vector3 _translation = { 0,0,0 };
		Vector3 _rotation = { 0, 0, 0 };
		float _scale = 1.0f;

	public:

		class OEntity* _parent = nullptr;

		virtual ~OElement() { }
	};

	class SPP_SCENE_API OEntity : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		std::vector<OElement*> _elements;

		OEntity(const MetaPath& InPath) : SPPObject(InPath) { }

	public:
		std::vector<OElement*>& GetElements()
		{
			return _elements;
		}

		virtual ~OEntity() { }
	};

	class SPP_SCENE_API OWorld : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		std::vector<OEntity*> _entities;

		OWorld(const MetaPath& InPath) : SPPObject(InPath) { }

	public:
		std::vector<OEntity*>& GetEntities()
		{
			return _entities;
		}
		virtual ~OWorld() { }
	};

	SPP_SCENE_API uint32_t GetSceneVersion();
}

