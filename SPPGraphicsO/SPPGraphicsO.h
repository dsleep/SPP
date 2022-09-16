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

		std::vector< std::shared_ptr< ArrayResource > > _rawMipData;
		std::shared_ptr< ImageMeta > _metaInfo;

		std::shared_ptr< class RT_Texture > _texture;


		virtual bool Finalize() override { UinitializeGraphicsDeviceResources(); return true; }

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


		virtual bool Finalize() override { UinitializeGraphicsDeviceResources(); return true; }

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

	class SPP_GRAPHICSO_API BaseMaterialParameter
	{
		RTTR_ENABLE();
		RTTR_REGISTRATION_FRIEND

	public:

		virtual BaseMaterialParameter* Clone() const = 0;
		virtual ~BaseMaterialParameter() {}
	};

	class ConstantParamter_Float : public BaseMaterialParameter
	{
		RTTR_ENABLE(BaseMaterialParameter);
		RTTR_REGISTRATION_FRIEND

	protected:
		float _value = 0.0f;

	public:
		ConstantParamter_Float() {}
		ConstantParamter_Float(float InValue) : _value(InValue) {}

		auto GetValue() { return _value; }

		virtual BaseMaterialParameter* Clone() const override
		{
			return new ConstantParamter_Float(_value);
		}

		virtual ~ConstantParamter_Float() {}
	};
	class ConstantParamter_Float2 : public BaseMaterialParameter
	{
		RTTR_ENABLE(BaseMaterialParameter);
		RTTR_REGISTRATION_FRIEND
	protected:
		Vector2 _value = { 0,0 };

	public:
		ConstantParamter_Float2() {}
		ConstantParamter_Float2(const Vector2 &InValue) : _value(InValue) {}

		auto GetValue() { return _value; }

		virtual BaseMaterialParameter* Clone() const override
		{
			return new ConstantParamter_Float2(_value);
		}

		virtual ~ConstantParamter_Float2() {}
	};
	class ConstantParamter_Float3 : public BaseMaterialParameter
	{
		RTTR_ENABLE(BaseMaterialParameter);
		RTTR_REGISTRATION_FRIEND

	protected:
		Vector3 _value = { 0,0,0 };

	public:
		ConstantParamter_Float3() {}
		ConstantParamter_Float3(const Vector3& InValue) : _value(InValue) {}

		auto GetValue() { return _value; }

		virtual BaseMaterialParameter* Clone() const override
		{
			return new ConstantParamter_Float3(_value);
		}

		virtual ~ConstantParamter_Float3() {}
	};
	class ConstantParamter_Float4 : public BaseMaterialParameter
	{
		RTTR_ENABLE(BaseMaterialParameter);
		RTTR_REGISTRATION_FRIEND

	protected:
		Vector4 _value = { 0,0,0,0 };

	public:
		ConstantParamter_Float4() {}
		ConstantParamter_Float4(const Vector4& InValue) : _value(InValue) {}

		auto GetValue() { return _value; }

		virtual BaseMaterialParameter* Clone() const override
		{
			return new ConstantParamter_Float4(_value);
		}

		virtual ~ConstantParamter_Float4() {}
	};

	class TextureParamater : public BaseMaterialParameter
	{
		RTTR_ENABLE(BaseMaterialParameter);
		RTTR_REGISTRATION_FRIEND
	
	protected:
		OTexture* _value = nullptr;

	public:
		TextureParamater() {}
		TextureParamater(OTexture* InValue) : _value(InValue) {}

		auto GetValue() { return _value; }

		virtual BaseMaterialParameter* Clone() const override
		{
			return new TextureParamater(_value);
		}

		virtual ~TextureParamater() {}
	};

	class MaterialParameterContainer
	{
		RTTR_ENABLE();
		RTTR_REGISTRATION_FRIEND

	protected:
		BaseMaterialParameter* _param = nullptr;

	public:
		MaterialParameterContainer() {}

		template<typename T>
		MaterialParameterContainer(T InValue) 
		{
			Set(InValue);
		}

		virtual ~MaterialParameterContainer()
		{
			FreeParam();
		}

		void FreeParam()
		{
			if (_param)
			{
				delete _param;
				_param = nullptr;
			}
		}

		void SetParamPtr(BaseMaterialParameter* InValue)
		{
			FreeParam();
			_param = InValue;
		}

		auto GetParam()
		{
			return _param;
		}

		MaterialParameterContainer(MaterialParameterContainer const& InParam) noexcept
		{
			if (InParam._param)
			{
				_param = InParam._param->Clone();
			}
		}
		MaterialParameterContainer& operator=(MaterialParameterContainer const& InParam) noexcept
		{
			if (InParam._param)
			{
				_param = InParam._param->Clone();
			}
			return *this;
		}

		MaterialParameterContainer(MaterialParameterContainer&& InParam) noexcept
		{
			std::swap(_param, InParam._param);			
		}

		MaterialParameterContainer& operator=(MaterialParameterContainer&& InParam) noexcept
		{
			std::swap(_param, InParam._param);			
			return *this;
		}

		void Set(const float &InValue)
		{
			SetParamPtr(new ConstantParamter_Float(InValue));
		}
		void Set(const Vector2 &InValue)
		{
			SetParamPtr(new ConstantParamter_Float2(InValue));
		}
		void Set(const Vector3& InValue)
		{
			SetParamPtr(new ConstantParamter_Float3(InValue));
		}
		void Set(const Vector4& InValue)
		{
			SetParamPtr(new ConstantParamter_Float4(InValue));
		}
		void Set(OTexture* InValue)
		{
			SetParamPtr(new TextureParamater(InValue));
		}
	};


	class SPP_GRAPHICSO_API OMaterial : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OMaterial(const std::string& InName, SPPDirectory* InParent) : SPPObject(InName, InParent) { }

		//std::vector<OShader*> _shaders;

		std::shared_ptr<RT_Material> _material;
		//std::map<TexturePurpose, OTexture*> _textures;

		std::map< std::string, MaterialParameterContainer > _parameters;

		virtual bool Finalize() override { UinitializeGraphicsDeviceResources(); return true; }

	public:
		virtual void InitializeGraphicsDeviceResources(GraphicsDevice* InOwner);
		virtual void UinitializeGraphicsDeviceResources();

		template<typename T>
		void SetParameter(const std::string& ParamName, const T &InValue)
		{
			_parameters[ParamName] = MaterialParameterContainer(InValue);			
		}

		//void SetTexture(TexturePurpose InTP, OTexture* InTexture)
		//{
		//	_textures[InTP] = InTexture;
		//}
		//void SetMaterial(std::shared_ptr<RT_Material> InMat)
		//{
		//	_material = InMat;
		//}
		//std::vector<OShader*> &GetShaders()
		//{
		//	return _shaders;
		//}
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
		OMeshElement(const std::string& InName, SPPDirectory* InParent) : ORenderableElement(InName, InParent) 
		{
			_bIsStatic = true;
		}
		OMesh* _meshObj = nullptr;
		OMaterial* _materialObj = nullptr;

		std::shared_ptr<RT_RenderableMesh> _renderableMesh;

	public:
		void SetMesh(OMesh* InMesh);

		auto GetMesh()
		{
			return _meshObj;
		}
		void SetMaterial(OMaterial* InMat)
		{
			_materialObj = InMat;
		}
		auto GetMaterial()
		{
			return _materialObj;
		}
		virtual bool Intersect_Ray(const Ray& InRay, IntersectionInfo& oInfo) const override;

		virtual void UpdateSelection(bool IsSelected);
		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene() override;


		virtual ~OMeshElement() { }
	};

	class SPP_GRAPHICSO_API OLight : public ORenderableElement
	{
		RTTR_ENABLE(ORenderableElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OLight(const std::string& InName, SPPDirectory* InParent) : ORenderableElement(InName, InParent) {}

	public:
		virtual ~OLight() { }
	};

	class SPP_GRAPHICSO_API OSun : public OLight
	{
		RTTR_ENABLE(OLight);
		RTTR_REGISTRATION_FRIEND

	protected:
		OSun(const std::string& InName, SPPDirectory* InParent) : OLight(InName, InParent) {}

	public:
		virtual ~OSun() { }
	};

	class SPP_GRAPHICSO_API OPointLight : public OLight
	{
		RTTR_ENABLE(OLight);
		RTTR_REGISTRATION_FRIEND

	protected:
		OPointLight(const std::string& InName, SPPDirectory* InParent) : OLight(InName, InParent) {}

	public:
		virtual ~OPointLight() { }
	};

	class SPP_GRAPHICSO_API OSpotLight : public OLight
	{
		RTTR_ENABLE(OSpotLight);
		RTTR_REGISTRATION_FRIEND

	protected:
		OSpotLight(const std::string& InName, SPPDirectory* InParent) : OLight(InName, InParent) {}

	public:
		virtual ~OSpotLight() { }
	};

	SPP_GRAPHICSO_API uint32_t GetGraphicsSceneVersion();
}

