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

			RunOnRT([this]()
				{
					_renderMesh->Initialize();
				});
		}
	}

	void OMesh::UinitializeGraphicsDeviceResources()
	{
		SE_ASSERT(IsOnCPUThread());
		RunOnRT([_renderMesh=_renderMesh]()
			{
				//
			});
		_renderMesh.reset();
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
		
		// remove all children
		for (auto& child : childCopy)
		{
			RemoveChild(child);
		}

		InGraphicsDevice->AddScene(_renderScene.get());

		// add them back
		for (auto& child : childCopy)
		{
			AddChild(child);
		}

		SE_ASSERT(childCopy.size() == _children.size());
		// preserve order
		_children = childCopy;
	}

	void ORenderableScene::RemoveFromGraphicsDevice()
	{		
		std::vector<OElement*> childCopy = _children;

		// remove all children
		for (auto& child : childCopy)
		{
			RemoveChild(child);
		}

		if (_renderScene)
		{
			RunOnRT([_owningDevice = this->_owningDevice, _renderScene = this->_renderScene]()
				{
					_owningDevice->RemoveScene(_renderScene.get());
				});
			_renderScene.reset();
			_owningDevice = nullptr;
		}


		// add them back
		for (auto& child : childCopy)
		{
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

			Vector3d TopPosition = GetTopBeforeScene()->GetPosition();
			Vector3d FinalPosition = localToWorld.block<1, 3>(3, 0).cast<double>() + TopPosition;
			_bounds = Sphere();

			if (_meshObj)
			{
				auto curMesh = _meshObj->GetMesh();
				if (curMesh)
				{
					_bounds = curMesh->GetBounds();

					Vector3d newBoundsCenter = (ToVector4(_bounds.GetCenter().cast<float>()) * localToWorld).cast<double>().head<3>() + TopPosition;					
					float maxScale = std::fabs(std::max(std::max(localToWorld.block<1, 3>(0, 0).norm(), localToWorld.block<1, 3>(1, 0).norm()), localToWorld.block<1, 3>(2, 0).norm()));
										
					_bounds = Sphere(newBoundsCenter, _bounds.GetRadius() * maxScale);
				}
			}

			if (!_bounds)
			{
				_bounds = Sphere(FinalPosition, 1);
			}

			SE_ASSERT(_materialObj);

			_meshObj->InitializeGraphicsDeviceResources(sceneGD);
			_materialObj->InitializeGraphicsDeviceResources(sceneGD);

			_renderableMesh = sceneGD->CreateRenderableMesh();

			_renderableMesh->SetRenderableMeshArgs({
				.mesh = _meshObj->GetDeviceMesh(),
				.material = _materialObj->GetMaterial()
				});

			_renderableMesh->SetArgs({
				.position = FinalPosition,
				.eulerRotationYPR = _rotation,
				.scale = _scale,
				.bIsStatic = _bIsStatic,
				.bounds = _bounds
				});

			_renderableMesh->AddToRenderScene(thisRenderableScene->GetRenderScene());
		}
	}
	void OMeshElement::RemovedFromScene()
	{
		ORenderableElement::RemovedFromScene();

		if (_renderableMesh)
		{
			RunOnRT([_renderableMesh = this->_renderableMesh]()
			{
				_renderableMesh->RemoveFromRenderScene();
			});
			_renderableMesh.reset();
		}
	}

	void OMeshElement::SetMesh(OMesh* InMesh)
	{
		if (_meshObj == InMesh) return;
		_meshObj = InMesh;
		// update bounds...
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

	std::shared_ptr<IMaterialParameter> MatParameterToIMat(BaseMaterialParameter* InParam, GraphicsDevice* InGD)
	{
		static auto paramFloat = rttr::type::get<ConstantParamter_Float>();
		static auto paramFloat2 = rttr::type::get<ConstantParamter_Float2>();
		static auto paramFloat3 = rttr::type::get<ConstantParamter_Float3>();
		static auto paramFloat4 = rttr::type::get<ConstantParamter_Float4>();
		static auto paramTexture = rttr::type::get<TextureParamater>();

		auto curType = InParam->get_type();

		if (curType == paramFloat)
		{
			auto castedValue = rttr::rttr_cast<ConstantParamter_Float*>(InParam);
			return std::make_shared< FloatParamter >( castedValue->GetValue() );
		}
		else if (curType == paramFloat2)
		{
			auto castedValue = rttr::rttr_cast<ConstantParamter_Float2*>(InParam);
			return std::make_shared< Float2Paramter >(castedValue->GetValue());
		}
		else if (curType == paramFloat3)
		{
			auto castedValue = rttr::rttr_cast<ConstantParamter_Float3*>(InParam);
			return std::make_shared< Float3Paramter >(castedValue->GetValue());
		}
		else if (curType == paramFloat4)
		{
			auto castedValue = rttr::rttr_cast<ConstantParamter_Float4*>(InParam);
			return std::make_shared< Float4Paramter >(castedValue->GetValue());
		}
		else if (curType == paramTexture)
		{
			auto castedValue = rttr::rttr_cast<TextureParamater*>(InParam);
			auto thisTexture = castedValue->GetValue();
			if (thisTexture)
			{
				thisTexture->InitializeGraphicsDeviceResources(InGD);
			}
			return thisTexture->GetDeviceTexture();
		}

		return nullptr;
	}

	void OMaterial::InitializeGraphicsDeviceResources(GraphicsDevice* InGD)
	{
		if (!_material)
		{
			std::map< std::string, std::shared_ptr<IMaterialParameter> > parameterMap;

			for (auto& [key, value] : _parameters)
			{
				auto imapParam = MatParameterToIMat(value.GetParam(), InGD);
				
				if (imapParam)
				{					
					parameterMap[key] = imapParam;
				}
			}

			_material = InGD->CreateMaterial();
			_material->SetMaterialArgs(
				{
					.parameterMap = parameterMap
				}
			);
		}
	}
	void OMaterial::UinitializeGraphicsDeviceResources()
	{
		SE_ASSERT(IsOnCPUThread());
		RunOnRT([_material=_material]()
			{
				//dies automagically 
			});
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

			RunOnRT([this]()
				{
					_texture->Initialize(_width, _height, _format, _rawImgData);
				});
		}
	}

	void OTexture::UinitializeGraphicsDeviceResources()
	{
		SE_ASSERT(IsOnCPUThread());
		RunOnRT([_texture = _texture]()
			{
				//dies automagically 
			});
		_texture.reset();
	}

	void OShader::InitializeGraphicsDeviceResources(GraphicsDevice* InOwner)
	{
		if (!_shader)
		{
			_shader = InOwner->CreateShader();

			RunOnRT([shader =_shader,
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
		.property("_parameters", &OMaterial::_parameters)(rttr::policy::prop::as_reference_wrapper)
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

	// paramters
	rttr::registration::class_<BaseMaterialParameter>("BaseMaterialParameter");

	rttr::registration::class_<ConstantParamter_Float>("ConstantParamter_Float")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_value", &ConstantParamter_Float::_value)(rttr::policy::prop::as_reference_wrapper)
		;
	rttr::registration::class_<ConstantParamter_Float2>("ConstantParamter_Float2")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		.property("_value", &ConstantParamter_Float2::_value)(rttr::policy::prop::as_reference_wrapper)
		;
	rttr::registration::class_<ConstantParamter_Float3>("ConstantParamter_Float3")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_value", &ConstantParamter_Float3::_value)(rttr::policy::prop::as_reference_wrapper)
		;
	rttr::registration::class_<ConstantParamter_Float4>("ConstantParamter_Float4")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_value", &ConstantParamter_Float4::_value)(rttr::policy::prop::as_reference_wrapper)
		;
	rttr::registration::class_<TextureParamater>("TextureParamater")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_value", &TextureParamater::_value)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<MaterialParameterContainer>("MaterialParameterContainer")
		.constructor<>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		.property("_param", &MaterialParameterContainer::_param)(rttr::policy::prop::as_reference_wrapper)
		;
}