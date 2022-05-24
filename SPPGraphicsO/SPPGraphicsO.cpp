// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphicsO.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	uint32_t GetGraphicsSceneVersion()
	{
		return 1;
	}

	ORenderableScene::ORenderableScene(const std::string& InName, SPPDirectory* InParent) : OScene(InName, InParent)
	{
		_renderScene = GGD()->CreateRenderScene();
	}

	void ORenderableElement::UpdateSelection(bool IsSelected)
	{
		_selected = IsSelected;
	}

	void ORenderableElement::AddedToScene(class OScene* InScene)
	{

	}

	void ORenderableElement::RemovedFromScene(class OScene* InScene)
	{

	}

	void OMeshElement::UpdateSelection(bool IsSelected)
	{
		if (_renderableMesh)
		{
			_renderableMesh->SetSelected(IsSelected);
		}
	}

	void OMeshElement::AddedToScene(class OScene* InScene)
	{
		if (!InScene) return;

		auto SceneType = InScene->get_type();
		if (_meshObj &&
			_meshObj->GetMesh() &&
			!_meshObj->GetMesh()->GetMeshElements().empty() &&
			SceneType.is_derived_from(rttr::type::get<ORenderableScene>()))
		{
			auto thisRenderableScene = (ORenderableScene*)InScene;

			auto firstMesh = _meshObj->GetMesh()->GetMeshElements().front();

			_renderableMesh = GGD()->CreateStaticMesh();

			auto localToWorld = GenerateLocalToWorld(true);	

			SE_ASSERT(_materialObj);

			std::vector<VertexStream> vertexStreams;

			MeshVertex placeholder;
			vertexStreams.push_back(CreateVertexStream(placeholder, placeholder.position, placeholder.normal, placeholder.texcoord, placeholder.color));

			_renderableMesh->SetMeshArgs({
				.vertexStreams = vertexStreams,
				.vertexResource = firstMesh->VertexResource,
				.indexResource = firstMesh->IndexResource,
				.material = _materialObj->GetMaterial(),
				});

			_renderableMesh->SetArgs({
				.position = localToWorld.block<1, 3>(3, 0).cast<double>() + GetTop()->GetPosition(),
				.eulerRotationYPR = _rotation,
				.scale = _scale,
				});

			_renderableMesh->AddToScene(thisRenderableScene->GetRenderScene());
		}
	}
	void OMeshElement::RemovedFromScene(class OScene* InScene)
	{
		if (!InScene) return;

		_renderableMesh->RemoveFromScene();
		_renderableMesh.reset();
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
		.property("_meshObj", &OMeshElement::_meshObj)
		;

	rttr::registration::class_<OMaterial>("OMaterial")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}