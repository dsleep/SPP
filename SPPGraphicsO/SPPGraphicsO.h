// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPSceneO.h"
#include "SPPGraphics.h"
#include "SPPMesh.h"

#if _WIN32 && !defined(SPP_GRAPHICSSCENE_STATIC)

	#ifdef SPP_GRAPHICSO_EXPORT
		#define SPP_GRAPHICSSCENE_API __declspec(dllexport)
	#else
		#define SPP_GRAPHICSSCENE_API __declspec(dllimport)
	#endif

	#else

		#define SPP_GRAPHICSSCENE_API 

#endif

namespace SPP
{
	class SPP_SCENE_API OMesh : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OMesh(const MetaPath& InPath) : SPPObject(InPath) { }
		std::shared_ptr<Mesh> _mesh;

	public:
		virtual ~OMesh() { }
	};

	class SPP_SCENE_API OMeshElement : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OMeshElement(const MetaPath& InPath) : OElement(InPath) { }
		OMesh* _meshObject = nullptr;

	public:
		virtual ~OMeshElement() { }
	};

	SPP_GRAPHICSSCENE_API uint32_t GetGraphicsSceneVersion();
}

