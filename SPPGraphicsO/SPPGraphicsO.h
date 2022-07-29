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
	class SPP_GRAPHICSO_API ODebugScene : public OScene
	{
		RTTR_ENABLE(OScene);
		RTTR_REGISTRATION_FRIEND

	protected:
		ODebugScene(const std::string& InName, SPPDirectory* InParent) : OScene(InName, InParent)
		{
		}
	
	public:

		virtual ~ODebugScene() { }
	};

	class SPP_GRAPHICSO_API ORenderableScene : public OScene
	{
		RTTR_ENABLE(OScene);
		RTTR_REGISTRATION_FRIEND

	protected:
		ORenderableScene(const std::string& InName, SPPDirectory* InParent);
		std::shared_ptr<RT_RenderScene> _renderScene;
		class GraphicsDevice* _owningDevice = nullptr;

	public:
		RT_RenderScene* GetRenderScene()
		{
			return _renderScene.get();
		}

		class GraphicsDevice *GetGraphicsDevice()
		{
			return _owningDevice;
		}

		virtual void AddToGraphicsDevice(GraphicsDevice* InGraphicsDevice);
		virtual void RemoveFromGraphicsDevice();

		virtual ~ORenderableScene();
	};

	class SPP_GRAPHICSO_API OTexture : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		int32_t _width;
		int32_t _height;
		TextureFormat _format;

		std::shared_ptr< ArrayResource > _rawImgData;
		std::shared_ptr< ImageMeta > _metaInfo;

		std::shared_ptr< class RT_Texture > _texture;

	public:
		OTexture(const std::string& InName, SPPDirectory* InParent) : SPPObject(InName, InParent) { }

		bool LoadFromDisk(const char *FileName);

		std::shared_ptr< class RT_Texture > GetDeviceTexture()
		{
			return _texture;
		}

		virtual ~OTexture() { }

		virtual void InitializeGraphicsDeviceResources(GraphicsDevice* InOwner);
		virtual void UinitializeGraphicsDeviceResources();
	};

	class SPP_GRAPHICSO_API OMesh : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OMesh(const std::string& InName, SPPDirectory* InParent) : SPPObject(InName, InParent) { }
		
		std::shared_ptr<Mesh> _mesh;
		std::shared_ptr<RT_StaticMesh> _renderMesh;

	public:
		void SetMesh(std::shared_ptr<Mesh> InMesh)
		{
			_mesh = InMesh;
		}
		std::shared_ptr<Mesh> GetMesh()
		{
			return _mesh;
		}

		virtual void InitializeGraphicsDeviceResources(GraphicsDevice* InOwner);
		virtual void UinitializeGraphicsDeviceResources();

		std::shared_ptr<RT_StaticMesh> GetDeviceMesh()
		{
			return _renderMesh;
		}

		virtual ~OMesh() { }
	};

	class SPP_GRAPHICSO_API OShader : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OShader(const std::string& InName, SPPDirectory* InParent) : SPPObject(InName, InParent) { }

		EShaderType _shaderType;
		std::string _filePath;
		std::string _entryPoint;
		std::shared_ptr<RT_Shader> _shader;

	public:
		void Initialize(EShaderType InType, const char *InFilePath, const char* EntryPoint = "main")
		{
			_shaderType = InType;
			_filePath = InFilePath;
			_entryPoint = EntryPoint;
		}
		virtual ~OShader() { }

		EShaderType GetShaderType() const
		{
			return _shaderType;
		}
		
		std::shared_ptr<RT_Shader> GetShader()
		{
			return _shader;
		}

		virtual void InitializeGraphicsDeviceResources(GraphicsDevice* InOwner);
		virtual void UinitializeGraphicsDeviceResources();
	};

	class SPP_GRAPHICSO_API OMaterial : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OMaterial(const std::string& InName, SPPDirectory* InParent) : SPPObject(InName, InParent) { }

		std::vector<OShader*> _shaders;
		std::vector<OTexture*> _textures;

		std::shared_ptr<RT_Material> _material;

	public:
		virtual void InitializeGraphicsDeviceResources(GraphicsDevice* InOwner);
		virtual void UinitializeGraphicsDeviceResources();

		void SetTexture(OTexture* InTexture, uint8_t CurrentIdx)
		{
			if (_textures.size() <= CurrentIdx)
			{
				_textures.resize(CurrentIdx + 1);
			}
			_textures[CurrentIdx] = InTexture;
		}
		void SetMaterial(std::shared_ptr<RT_Material> InMat)
		{
			_material = InMat;
		}
		std::vector<OShader*> &GetShaders()
		{
			return _shaders;
		}
		std::shared_ptr<RT_Material> GetMaterial()
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
		ORenderableElement(const std::string& InName, SPPDirectory* InParent) : OElement(InName, InParent) { }
		bool _selected = false;

	public:
		virtual void UpdateSelection(bool IsSelected);			
		virtual ~ORenderableElement() { }
	};



	class SPP_GRAPHICSO_API OMeshElement : public ORenderableElement
	{
		RTTR_ENABLE(ORenderableElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OMeshElement(const std::string& InName, SPPDirectory* InParent) : ORenderableElement(InName, InParent) { }
		OMesh* _meshObj = nullptr;
		OMaterial* _materialObj = nullptr;

		std::shared_ptr<RT_RenderableMesh> _renderableMesh;

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
		virtual void RemovedFromScene() override;


		virtual ~OMeshElement() { }
	};

	SPP_GRAPHICSO_API uint32_t GetGraphicsSceneVersion();
}

