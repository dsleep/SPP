// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPSceneO.h"
#include "SPPMesh.h"

#if _WIN32 && !defined(SPP_SDF_STATIC)

	#ifdef SPP_SDFO_EXPORT
		#define SPP_SDF_API __declspec(dllexport)
	#else
		#define SPP_SDF_API __declspec(dllimport)
	#endif

#else

	#define SPP_SDF_API 

#endif


namespace SPP
{		
	class SPP_SDF_API OShape : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		EShapeType _shapeType = EShapeType::Unknown;
		EShapeOp _shapeOp = EShapeOp::Add;

		OShape(const MetaPath& InPath) : OElement(InPath) { }

	public:
		virtual SDFShape GenerateShape() const
		{
			SDFShape oShape;
			oShape.shapeType = _shapeType;
			return oShape;
		}

		virtual ~OShape() { }
	};

	class SPP_SDF_API OShapeGroup : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OShapeGroup(const MetaPath& InPath) : OElement(InPath) { }
		std::shared_ptr<RenderableSignedDistanceField> _renderableSDF;
		std::vector<SDFShape> _shapeCache;

		void _GenerateShapes();

	public:
		
		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene(class OScene* InScene) override;

		virtual ~OShapeGroup() { }
	};

	class SPP_SDF_API OSDFSphere : public OShape
	{
		RTTR_ENABLE(OShape);
		RTTR_REGISTRATION_FRIEND

	protected:
		OSDFSphere(const MetaPath& InPath) : OShape(InPath) 
		{
			_shapeType = EShapeType::Sphere;
		}
		float _radius = 1.0f;

	public:		
		virtual SDFShape GenerateShape() const
		{
			SDFShape oShape;
			oShape.shapeType = _shapeType;
			oShape.translation = _translation.cast<float>();
			oShape.params[0] = _radius;
			return oShape;
		}
		void SetRadius(float InRadius);
		virtual ~OSDFSphere() { }
	};

	class SPP_SDF_API OSDFBox : public OShape
	{
		RTTR_ENABLE(OShape);
		RTTR_REGISTRATION_FRIEND

	protected:
		OSDFBox(const MetaPath& InPath) : OShape(InPath) 
		{
			_shapeType = EShapeType::Box;
		}
		Vector3 _extents = { 1, 1, 1 };
	public:
		virtual ~OSDFBox() { }
	};

	SPP_SDF_API uint32_t GetSDFVersion();
}