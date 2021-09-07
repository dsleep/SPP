#include "SPPSDF.h"


namespace SPP
{
	uint32_t GetSDFVersion()
	{
		return 1;
	}
}

using namespace SPP;

RTTR_REGISTRATION
{
	rttr::registration::enumeration<EShapeOp>("EShapeOp")
				  (
					  rttr::value("Add",		EShapeOp::Add),
					  rttr::value("Subtract",   EShapeOp::Subtract),
					  rttr::value("Intersect",	EShapeOp::Intersect),
					  rttr::value("SmoothAdd",	EShapeOp::SmoothAdd)
				  );

	rttr::registration::class_<OElement>("OElement")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_parent", &OElement::_parent)
		;

	rttr::registration::class_<OEntity>("OEntity")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_elements", &OEntity::_elements)
		;


	rttr::registration::class_<OShape>("OShape")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_shapeOp", &OShape::_shapeOp)
		;

	rttr::registration::class_<OShapeGroup>("OShapeGroup")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_shapes", &OShapeGroup::_shapes)
		;
}
