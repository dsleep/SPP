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
		Color3 _shapeColor = Color3(127, 127, 127);

		OShape(const std::string& InName, SPPDirectory* InParent) : OElement(InName, InParent) { }

	public:

		struct Args
		{
			EShapeType shapeType;
			EShapeOp shapeOp;
			float shapeBlendFactor;
			Color3 shapeColor;
		};

		void SetShapeArgs(const Args &InArgs)
		{
			_shapeType = InArgs.shapeType;
			_shapeOp = InArgs.shapeOp;
			_shapeBlendFactor = InArgs.shapeBlendFactor;
			_shapeColor = InArgs.shapeColor;			
		}

		virtual SDFShape GenerateShape() const
		{
			SDFShape oShape;
			oShape.shapeType = _shapeType;
			oShape.shapeOp = _shapeOp;

			auto currentLocalToWorld = GenerateLocalToWorld(true);

			oShape.invTransform = currentLocalToWorld.inverse();
			oShape.shapeParams[0] = _shapeBlendFactor;
			oShape.shapeColor = Vector3(_shapeColor[0] / 255.0f, _shapeColor[1] / 255.0f, _shapeColor[2] / 255.0f);

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

	SPP_SDF_API uint32_t GetSDFVersion();
}