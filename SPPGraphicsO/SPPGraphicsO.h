// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPSceneO.h"
#include "SPPGraphics.h"
#include "SPPTextures.h"
#include "SPPMesh.h"
#include "SPPSceneRendering.h"

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
		std::shared_ptr<GD_RenderScene> _renderScene;

	public:
		GD_RenderScene* GetRenderScene()
		{
			return _renderScene.get();
		}

		std::shared_ptr<GD_RenderScene> GetRenderSceneShared()
		{
			return _renderScene;
		}

		virtual ~ORenderableScene() { }
	};

	class SPP_GRAPHICSO_API OTexture : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		std::unique_ptr<TextureAsset> _texture;

	public:
		OTexture(const MetaPath& InPath) : SPPObject(InPath) { }

		virtual ~OTexture() { }
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

	class SPP_GRAPHICSO_API OMaterial : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OMaterial(const MetaPath& InPath) : SPPObject(InPath) { }

		std::shared_ptr<GD_Material> _material;

	public:
		void SetMaterial(std::shared_ptr<GD_Material> InMat)
		{
			_material = InMat;
		}
		std::shared_ptr<GD_Material> GetMaterial()
		{
			return _material;
		}
		virtual ~OMaterial() { }
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
		OMaterial* _materialObj = nullptr;

		std::shared_ptr<GD_RenderableMesh> _renderableMesh;

	public:
		void SetMesh(OMesh* InMesh)
		{
			_meshObj = InMesh;
		}
		void SetMaterial(OMaterial* InMat)
		{
			_materialObj = InMat;
		}
		virtual bool Intersect_Ray(const Ray& InRay, IntersectionInfo& oInfo) const override;

		virtual void UpdateSelection(bool IsSelected);
		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene(class OScene* InScene) override;

		virtual ~OMeshElement() { }
	};

	SPP_GRAPHICSO_API uint32_t GetGraphicsSceneVersion();
}

