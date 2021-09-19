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

	void OElement::UpdateTransform()
	{
		OElement* lastFromTop = this;
		OElement* top = _parent;
		while (top)
		{
			if (top->_parent)
			{
				lastFromTop = top;
				top = top->_parent;
			}
			else
			{
				break;
			}
		}

		SE_ASSERT(top);

		auto SceneType = top->get_type();
		if (SceneType.is_derived_from(rttr::type::get<OScene>()))
		{
			OScene* topScene = (OScene*)top;
			topScene->RemoveChild(lastFromTop);
			topScene->AddChild (lastFromTop);
		}
	}

	OElement* OElement::GetTop() 
	{
		if (_parent)
		{
			return _parent->GetTop();
		}
		else
		{
			return this;
		}
	}

	Matrix4x4 OElement::GenerateLocalToWorld(bool bSkipTopTranslation) const
	{
		const float degToRad = 0.0174533f;

		Eigen::AngleAxisf yawAngle(_rotation[0] * degToRad, Vector3::UnitY());
		Eigen::AngleAxisf pitchAngle(_rotation[1] * degToRad, Vector3::UnitX());
		Eigen::AngleAxisf rollAngle(_rotation[2] * degToRad, Vector3::UnitZ());
		Eigen::Quaternion<float> q = rollAngle * yawAngle * pitchAngle;

		Matrix3x3 scaleMatrix = Matrix3x3::Identity();
		scaleMatrix(0, 0) = _scale;
		scaleMatrix(1, 1) = _scale;
		scaleMatrix(2, 2) = _scale;
		Matrix3x3 rotationMatrix = q.matrix();

		Matrix4x4 transform = Matrix4x4::Identity();
		transform.block<3, 3>(0, 0) = scaleMatrix * rotationMatrix;
		transform.block<1, 3>(3, 0) = (bSkipTopTranslation && _parent == nullptr) ?
			Vector3(0, 0, 0) :
			Vector3(_translation[0], _translation[1], _translation[2]);

		if (_parent)
		{
			transform = transform * _parent->GenerateLocalToWorld();
		}

		return transform;
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

	rttr::registration::class_<Vector3d>("Vector3d")
		.property("x", &Vector3d::GetX, &Vector3d::SetX)
		.property("y", &Vector3d::GetY, &Vector3d::SetY)
		.property("z", &Vector3d::GetZ, &Vector3d::SetZ)
		;

	rttr::registration::class_<Vector4d>("Vector4d")
		.property("x", &Vector4d::GetX, &Vector4d::SetX)
		.property("y", &Vector4d::GetY, &Vector4d::SetY)
		.property("z", &Vector4d::GetZ, &Vector4d::SetZ)
		.property("w", &Vector4d::GetW, &Vector4d::SetW)
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