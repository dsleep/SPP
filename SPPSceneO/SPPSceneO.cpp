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