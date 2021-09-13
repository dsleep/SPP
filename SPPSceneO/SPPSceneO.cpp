// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPSceneO.h"

namespace SPP
{
	uint32_t GetSceneVersion()
	{
		return 1;
	}

	void OElement::AddChild(OElement* InChild)
	{
		InChild->RemoveFromParent();
		_children.insert(InChild);
		InChild->_parent = this;
	}

	void OElement::RemoveChild(OElement* InChild)
	{
		SE_ASSERT(InChild->_parent == this);
		_children.erase(InChild);
		InChild->_parent = nullptr;
	}

	void OElement::RemoveFromParent()
	{
		if (_parent)
		{
			_parent->RemoveChild(this);
		}
	}

	bool OElement::Intersect_Ray(const Ray& InRay, IntersectionInfo& oInfo) const
	{
		if (_bounds)
		{
			return Intersection::Intersect_RaySphere(InRay, _bounds, oInfo.location);
		}
		return false;
	}

	OScene::OScene(const MetaPath& InPath) : OElement(InPath) 
	{ 
		_octree = std::make_unique<LooseOctree>();
		_octree->Initialize(Vector3d(0, 0, 0), 100000, 1);
	}

	void OScene::AddChild(OElement* InChild)
	{
		OElement::AddChild(InChild);
		if (InChild->Bounds())
		{
			_octree->AddElement(InChild);
		}
		InChild->AddedToScene(this);
	}
	void OScene::RemoveChild(OElement* InChild)
	{
		OElement::RemoveChild(InChild);
		_octree->AddElement(InChild);

		if (InChild->IsInOctree())
		{
			_octree->RemoveElement(InChild);
		}
		InChild->RemovedFromScene(this);
	}
}

using namespace SPP;

RTTR_REGISTRATION
{	
	rttr::registration::class_<Vector3>("Vector3")
		.property("x", &Vector3::GetX, &Vector3::SetX)
		.property("y", &Vector3::GetY, &Vector3::SetY)
		.property("z", &Vector3::GetZ, &Vector3::SetZ)
		;

	rttr::registration::class_<OElement>("OElement")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_parent", &OElement::_parent)
		.property("_children", &OElement::_children)
		.property("_translation", &OElement::_translation)
		.property("_rotation", &OElement::_rotation)
		.property("_scale", &OElement::_scale)
		;

	rttr::registration::class_<OScene>("OScene")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		;
}