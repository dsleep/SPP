// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPSceneO.h"
#include "SPPGraphics.h"
#include "SPPMesh.h"

#if _WIN32 && !defined(SPP_GRAPHICSO_STATIC)

	#ifdef SPP_GRAPHICSO_EXPORT
		#define SPP_GRAPHICSO_API __declspec(dllexport)
	#else
		#define SPP_GRAPHICSO_API __declspec(dllimport)
	#endif

	#else

		#define SPP_GRAPHICSO_API 

#endif

namespace SPP
{
	class SPP_GRAPHICSO_API ORenderableScene : public OScene
	{
		RTTR_ENABLE(OScene);
		RTTR_REGISTRATION_FRIEND

	protected:
		ORenderableScene(const MetaPath& InPath);
		std::shared_ptr<RenderScene> _renderScene;

	public:
		RenderScene* GetRenderScene()
		{
			return _renderScene.get();
		}

		virtual ~ORenderableScene() { }
	};

	class SPP_GRAPHICSO_API OTexture : public SPPObject
	{

	};

	class SPP_GRAPHICSO_API OMesh : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OMesh(const MetaPath& InPath) : SPPObject(InPath) { }
		std::shared_ptr<Mesh> _mesh;

	public:
		void SetMesh(std::shared_ptr<Mesh> InMesh)
		{
			_mesh = InMesh;
		}
		std::shared_ptr<Mesh> GetMesh()
		{
			return _mesh;
		}
		virtual ~OMesh() { }
	};

	class SPP_GRAPHICSO_API ORenderableElement : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		ORenderableElement(const MetaPath& InPath) : OElement(InPath) { }
		bool _selected = false;

	public:
		virtual void UpdateSelection(bool IsSelected);
		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene(class OScene* InScene) override;

		virtual ~ORenderableElement() { }
	};

	class SPP_GRAPHICSO_API OMeshElement : public ORenderableElement
	{
		RTTR_ENABLE(ORenderableElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OMeshElement(const MetaPath& InPath) : ORenderableElement(InPath) { }
		OMesh* _meshObj = nullptr;

		std::shared_ptr<RenderableMesh> _renderableMesh;

	public:
		void SetMesh(OMesh* InMesh)
		{
			_meshObj = InMesh;
		}
		virtual bool Intersect_Ray(const Ray& InRay, IntersectionInfo& oInfo) const override;

		virtual void UpdateSelection(bool IsSelected);
		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene(class OScene* InScene) override;

		virtual ~OMeshElement() { }
	};

	SPP_GRAPHICSO_API uint32_t GetGraphicsSceneVersion();
}

