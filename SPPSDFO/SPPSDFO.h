// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPSceneO.h"
#include "SPPMesh.h"
#include "SPPSceneRendering.h"

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
		float _shapeBlendFactor = 0.0f;

		OShape(const std::string& InName, SPPDirectory* InParent) : OElement(InName, InParent) { }

	public:

		struct Args
		{
			EShapeType shapeType;
			EShapeOp shapeOp;
			float shapeBlendFactor;
		};

		void SetShapeArgs(const Args &InArgs)
		{
			_shapeType = InArgs.shapeType;
			_shapeOp = InArgs.shapeOp;
			_shapeBlendFactor = InArgs.shapeBlendFactor;
		}

		virtual SDFShape GenerateShape() const
		{
			SDFShape oShape;
			oShape.shapeType = _shapeType;
			oShape.shapeOp = _shapeOp;
			oShape.shapeBlendAndScale[0] = _shapeBlendFactor;
			oShape.translation = _translation.cast<float>();
			return oShape;
		}

		virtual ~OShape() { }
	};

	class SPP_SDF_API OShapeGroup : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OShapeGroup(const std::string& InName, SPPDirectory* InParent) : OElement(InName, InParent) { }
		std::shared_ptr<GD_RenderableSignedDistanceField> _renderableSDF;
		std::vector<SDFShape> _shapeCache;
		Vector3 _color = { 0.5f,0.5f,0.5f };
		GPUReferencer< GPUShader > _shaderOverride;

		void _GenerateShapes();

	public:

		void SetCustomShader(GPUReferencer< GPUShader > &InShader)
		{
			_shaderOverride = InShader;
		}
		
		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene() override;

		virtual ~OShapeGroup() { }
	};

	class SPP_SDF_API OSDFSphere : public OShape
	{
		RTTR_ENABLE(OShape);
		RTTR_REGISTRATION_FRIEND

	protected:
		OSDFSphere(const std::string& InName, SPPDirectory* InParent) : OShape(InName, InParent)
		{
			_shapeType = EShapeType::Sphere;
		}
		float _radius = 1.0f;

	public:		
		virtual SDFShape GenerateShape() const
		{
			SDFShape oShape;
			oShape.shapeType = _shapeType;
			oShape.shapeOp = _shapeOp;
			oShape.translation = _translation.cast<float>();
			oShape.params[0] = _radius;
			oShape.shapeBlendAndScale[0] = _shapeBlendFactor;
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
		OSDFBox(const std::string& InName, SPPDirectory* InParent) : OShape(InName, InParent)
		{
			_shapeType = EShapeType::Box;
		}
		Vector3 _extents = { 1, 1, 1 };
	public:
		virtual SDFShape GenerateShape() const
		{
			SDFShape oShape;
			oShape.shapeType = _shapeType;
			oShape.shapeOp = _shapeOp;
			oShape.translation = _translation.cast<float>();
			oShape.params[0] = _extents[0];
			oShape.params[1] = _extents[1];
			oShape.params[2] = _extents[2];
			oShape.shapeBlendAndScale[0] = _shapeBlendFactor;
			return oShape;
		}
		void SetExtents(Vector3 InExtents)
		{
			_extents = InExtents;
		}
		virtual ~OSDFBox() { }
	};

	SPP_SDF_API uint32_t GetSDFVersion();
}