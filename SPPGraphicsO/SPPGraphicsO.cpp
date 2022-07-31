// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphicsO.h"
#include "ThreadPool.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	uint32_t GetGraphicsSceneVersion()
	{
		return 1;
	}

	const std::vector<VertexStream>& GetVertexStreams(const MeshVertex& InPlaceholder)
	{
		static std::vector<VertexStream> vertexStreams;
		if (vertexStreams.empty())
		{
			vertexStreams.push_back(
				CreateVertexStream(InPlaceholder, 
					InPlaceholder.position,
					InPlaceholder.normal, 
					InPlaceholder.texcoord[0], 
					InPlaceholder.texcoord[1], 
					InPlaceholder.color));
		}
		return vertexStreams;
	}

	void OMesh::InitializeGraphicsDeviceResources(GraphicsDevice* InOwner)
	{
		if (!_renderMesh)
		{
			auto firstMesh = GetMesh()->GetMeshElements().front();

			_renderMesh = InOwner->CreateStaticMesh();

			_renderMesh->SetMeshArgs({
				.vertexStreams = GetVertexStreams(MeshVertex{}),
				.vertexResource = firstMesh->VertexResource,
				.indexResource = firstMesh->IndexResource
				});

			GPUThreaPool->enqueue([this]()
				{
					_renderMesh->Initialize();
				});
		}
	}

	void OMesh::UinitializeGraphicsDeviceResources()
	{

	}

	ORenderableScene::ORenderableScene(const std::string& InName, SPPDirectory* InParent) : OScene(InName, InParent)
	{
	}

	ORenderableScene::~ORenderableScene()
	{
		_children.clear();

		if (_owningDevice)
		{
			if (_renderScene)
			{
				RemoveFromGraphicsDevice();
			}
		}
	}

	void ORenderableScene::AddToGraphicsDevice(GraphicsDevice *InGraphicsDevice)
	{
		_owningDevice = InGraphicsDevice;
		_renderScene = InGraphicsDevice->CreateRenderScene();

		std::vector<OElement*> childCopy = _children;
		
		// reinit properly
		for (auto& child : childCopy)
		{
			RemoveChild(child);
			AddChild(child);
		}

		SE_ASSERT(childCopy.size() == _children.size());
		// preserve order
		_children = childCopy;

		InGraphicsDevice->AddScene(_renderScene.get());
	}

	void ORenderableScene::RemoveFromGraphicsDevice()
	{		
		auto sceneRef = _renderScene;

		_owningDevice->RemoveScene(sceneRef.get());
		_owningDevice = nullptr;
		_renderScene.reset();
		
		std::vector<OElement*> childCopy = _children;

		// reinit properly
		for (auto& child : childCopy)
		{
			RemoveChild(child);
			AddChild(child);
		}

		SE_ASSERT(childCopy.size() == _children.size());
		// preserve order
		_children = childCopy;
	}

	void ORenderableElement::UpdateSelection(bool IsSelected)
	{
		_selected = IsSelected;
	}

	void OMeshElement::UpdateSelection(bool IsSelected)
	{
		//if (_renderableMesh)
		//{
		//	_renderableMesh->SetSelected(IsSelected);
		//}
	}

	void OMeshElement::AddedToScene(class OScene* InScene)
	{
		ORenderableElement::AddedToScene(InScene);

		if (!InScene) return; 

		auto thisRenderableScene = dynamic_cast<ORenderableScene*>(InScene);
		SE_ASSERT(thisRenderableScene);

		auto SceneType = InScene->get_type();
		if (_meshObj &&
			_meshObj->GetMesh() &&
			!_meshObj->GetMesh()->GetMeshElements().empty())
		{
			// not ready yet
			if (!thisRenderableScene->GetGraphicsDevice() 
				|| !thisRenderableScene->GetRenderScene()) return;

			auto sceneGD = thisRenderableScene->GetGraphicsDevice();
			auto firstMesh = _meshObj->GetMesh()->GetMeshElements().front();
						
			auto localToWorld = GenerateLocalToWorld(true);	

			SE_ASSERT(_materialObj);

			_meshObj->InitializeGraphicsDeviceResources(sceneGD);
			_materialObj->InitializeGraphicsDeviceResources(sceneGD);

			_renderableMesh = sceneGD->CreateRenderableMesh();

			_renderableMesh->SetRenderableMeshArgs({
				.mesh = _meshObj->GetDeviceMesh(),
				.material = _materialObj->GetMaterial()
				});

			_renderableMesh->SetArgs({
				.position = localToWorld.block<1, 3>(3, 0).cast<double>() + GetTopBeforeScene()->GetPosition(),
				.eulerRotationYPR = _rotation,
				.scale = _scale,
				});

			_renderableMesh->AddToRenderScene(thisRenderableScene->GetRenderScene());
		}
	}
	void OMeshElement::RemovedFromScene()
	{
		ORenderableElement::RemovedFromScene();

		if (_renderableMesh)
		{
			GPUThreaPool->enqueue([_renderableMesh = this->_renderableMesh]()
			{
				_renderableMesh->RemoveFromRenderScene();
			});
			_renderableMesh.reset();
		}
	}

	bool OMeshElement::Intersect_Ray(const Ray& InRay, IntersectionInfo& oInfo) const
	{
		auto localToWorld = GenerateLocalToWorld();
		//if (_bounds)
		{
			if (_meshObj &&
				_meshObj->GetMesh() &&
				!_meshObj->GetMesh()->GetMeshElements().empty())
			{
				auto meshElements = _meshObj->GetMesh()->GetMeshElements();

				for (auto& curMesh : meshElements)
				{
					auto curBounds = curMesh->Bounds.Transform(localToWorld);

					if (curBounds)
					{
						if (Intersection::Intersect_RaySphere(InRay, curBounds, oInfo.location))
						{
							oInfo.hitName = curMesh->Name;
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	void OMaterial::InitializeGraphicsDeviceResources(GraphicsDevice* InGD)
	{
		if (!_material)
		{
			std::shared_ptr< RT_Shader > vertexShader;
			std::shared_ptr< RT_Shader > pixelShader;
			std::vector< std::shared_ptr<RT_Texture> > textureArray;

			for (auto& shader : _shaders)
			{
				shader->InitializeGraphicsDeviceResources(InGD);

				if (shader->GetShaderType() == EShaderType::Vertex)
				{
					vertexShader = shader->GetShader();
				}
				else if (shader->GetShaderType() == EShaderType::Pixel)
				{
					pixelShader = shader->GetShader();
				}
			}

			for (auto& texture : _textures)
			{
				texture->InitializeGraphicsDeviceResources(InGD);
				textureArray.push_back(texture->GetDeviceTexture());
			}

			_material = InGD->CreateMaterial();
			_material->SetMaterialArgs(
				{
					.vertexShader = vertexShader,
					.pixelShader = pixelShader,
					.textureArray = textureArray
				}
			);
		}
	}
	void OMaterial::UinitializeGraphicsDeviceResources()
	{
		_material.reset();
	}

	bool OTexture::LoadFromDisk(const char* FileName)
	{		
		TextureAsset testTexture;
		
		if (testTexture.LoadFromDisk(FileName))
		{
			_width = testTexture.width;
			_height = testTexture.height;
			_rawImgData = testTexture.rawImgData;
		}			

		return false;
	}

	void OTexture::InitializeGraphicsDeviceResources(GraphicsDevice* InOwner)
	{
		if (!_texture)
		{
			_texture = InOwner->CreateTexture();

			GPUThreaPool->enqueue([this]()
				{
					_texture->Initialize(_width, _height, _format, _rawImgData);
				});
		}
	}

	void OTexture::UinitializeGraphicsDeviceResources()
	{
		_texture.reset();
	}

	void OShader::InitializeGraphicsDeviceResources(GraphicsDevice* InOwner)
	{
		if (!_shader)
		{
			_shader = InOwner->CreateShader();

			GPUThreaPool->enqueue([shader =_shader, 
				shaderType = _shaderType, 
				filePath = _filePath,
				entryPoint = _entryPoint]()
				{
					shader->Initialize(shaderType);
					shader->CompileShaderFromFile(filePath.c_str(), entryPoint.c_str());
				});
		}
	}

	void OShader::UinitializeGraphicsDeviceResources()
	{
		_shader.reset();
	}
}

using namespace SPP;

RTTR_REGISTRATION
{	
	rttr::registration::class_<ORenderableScene>("ORenderableScene")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OMesh>("OMesh")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OTexture>("OTexture")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		;

	rttr::registration::class_<ORenderableElement>("ORenderableElement")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OMeshElement>("OMeshElement")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_meshObj", &OMeshElement::_meshObj)(rttr::policy::prop::as_reference_wrapper)
		.property("_materialObj", &OMeshElement::_materialObj)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<OMaterial>("OMaterial")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_shaders", &OMaterial::_shaders)(rttr::policy::prop::as_reference_wrapper)
		.property("_textures", &OMaterial::_textures)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<OShader>("OShader")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<ODebugScene>("ODebugScene")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}