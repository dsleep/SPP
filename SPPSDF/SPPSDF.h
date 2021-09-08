// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPObject.h"
#include "SPPMath.h"

#if _WIN32 && !defined(SPP_SDF_STATIC)

	#ifdef SPP_SDF_EXPORT
		#define SPP_SDF_API __declspec(dllexport)
	#else
		#define SPP_SDF_API __declspec(dllimport)
	#endif

#else

	#define SPP_SDF_API 

#endif


namespace SPP
{	
	class SPP_SDF_API OElement : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		OElement(const MetaPath& InPath) : SPPObject(InPath) { }

	public:

		class OEntity* _parent = nullptr;

		virtual ~OElement() { }
	};

	class SPP_SDF_API OEntity : public SPPObject
	{
		RTTR_ENABLE(SPPObject);
		RTTR_REGISTRATION_FRIEND

	protected:
		std::vector<OElement*> _elements;

		OEntity(const MetaPath& InPath) : SPPObject(InPath) { }

	public:
		virtual ~OEntity() { }
	};

	enum class EShapeOp
	{
		Add,
		Subtract,
		Intersect,
		SmoothAdd
	};

	class SPP_SDF_API OShape : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OShape(const MetaPath& InPath) : OElement(InPath) { }
		EShapeOp _shapeOp = EShapeOp::Add;

	public:
		virtual ~OShape() { }
	};

	class SPP_SDF_API OShapeGroup : public OEntity
	{
		RTTR_ENABLE(OEntity);
		RTTR_REGISTRATION_FRIEND

	protected:
		OShapeGroup(const MetaPath& InPath) : OEntity(InPath) { }

		std::vector< class OShape* > _shapes;

	public:
		virtual ~OShapeGroup() { }
	};

	class SPP_SDF_API OSDFSphere : public OShape
	{
		RTTR_ENABLE(OShape);
		RTTR_REGISTRATION_FRIEND

	protected:
		OSDFSphere(const MetaPath& InPath) : OShape(InPath) { }
		float _radius = 1.0f;

	public:
		void SetRadius(float InRadius);
		virtual ~OSDFSphere() { }
	};

	class SPP_SDF_API OSDFBox : public OShape
	{
		RTTR_ENABLE(OShape);
		RTTR_REGISTRATION_FRIEND

	protected:
		OSDFBox(const MetaPath& InPath) : OShape(InPath) { }
		Vector3 _extents = { 1, 1, 1 };

	public:
		virtual ~OSDFBox() { }
	};

	SPP_SDF_API uint32_t GetSDFVersion();
}