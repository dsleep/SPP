// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphicsO.h"

namespace SPP
{
	uint32_t GetGraphicsSceneVersion()
	{
		return 1;
	}

	ORenderableScene::ORenderableScene(const MetaPath& InPath) : OScene(InPath)
	{
		_renderScene = GGI()->CreateRenderScene();
	}

	void ORenderableElement::AddedToScene(class OScene* InScene)
	{

	}

	void ORenderableElement::RemovedFromScene(class OScene* InScene)
	{

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
			_renderableMesh = GGI()->CreateRenderableMesh();

			auto& pos = _renderableMesh->GetPosition();
			pos = _translation;
			auto& scale = _renderableMesh->GetScale();
			scale = Vector3(1, 1, 1);

			_renderableMesh->SetMeshData(_meshObj->GetMesh()->GetMeshElements());
			_renderableMesh->AddToScene(((ORenderableScene*)InScene)->GetRenderScene());
		}
	}
	void OMeshElement::RemovedFromScene(class OScene* InScene)
	{
		if (!InScene) return;
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