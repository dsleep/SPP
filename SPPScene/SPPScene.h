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

		class OElement* _parent = nullptr;
		std::vector<OElement*> _children;

		Vector3 _translation = { 0,0,0 };
		Vector3 _rotation = { 0, 0, 0 };
		float _scale = 1.0f;

	public:
		std::vector<OElement*>& GetChildElements()
		{
			return _children;
		}

		virtual ~OElement() { }
	};

	class SPP_SCENE_API OScene : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OScene(const MetaPath& InPath) : OElement(InPath) { }

	public:
		virtual ~OScene() { }
	};

	SPP_SCENE_API uint32_t GetSceneVersion();
}

