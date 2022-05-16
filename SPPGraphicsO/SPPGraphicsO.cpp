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

	ORenderableScene::ORenderableScene(const MetaPath& InPath) : OScene(InPath)
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
			_renderableMesh = GGD()->CreateStaticMesh();

			auto localToWorld = GenerateLocalToWorld(true);			 
			auto& pos = _renderableMesh->GetPosition();
			pos = localToWorld.block<1, 3>(3, 0).cast<double>() + GetTop()->GetPosition();
			auto& scale = _renderableMesh->GetScale();
			scale = Vector3(_scale, _scale, _scale);

			_renderableMesh->SetMeshData(_meshObj->GetMesh()->GetMeshElements());
			_renderableMesh->AddToScene(((ORenderableScene*)InScene)->GetRenderScene());
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
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OMesh>("OMesh")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OTexture>("OTexture")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		;

	rttr::registration::class_<ORenderableElement>("ORenderableElement")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;

	rttr::registration::class_<OMeshElement>("OMeshElement")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_meshObj", &OMeshElement::_meshObj)
		;

}