#include "SPPSDFO.h"
#include "SPPGraphicsO.h"
#include "ThreadPool.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	float opUnion(float d1, float d2) {
		return std::min(d1, d2);
	}

	float opSubtraction(float d1, float d2) {
		return std::max(-d1, d2);
	}

	float opIntersection(float d1, float d2) {
		return std::max(d1, d2);
	}

	float opSmoothUnion(float d1, float d2, float k) {
		float h = std::clamp(0.5f + 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
		return std::lerp(d2, d1, h) - k * h * (1.0f - h);
	}

	float opSmoothSubtraction(float d1, float d2, float k) {
		float h = std::clamp(0.5f - 0.5f * (d2 + d1) / k, 0.0f, 1.0f);
		return std::lerp(d2, -d1, h) + k * h * (1.0f - h);
	}

	float opSmoothIntersection(float d1, float d2, float k)
	{
		float h = std::clamp(0.5f - 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
		return std::lerp(d2, d1, h) + k * h * (1.0f - h);
	}

	//unit sphere
	float sdSphere(const Vector3 &p)
	{
		return p.norm() - 1.0f;
	}

	//1,1,1 box
	float sdBox(const Vector3 &p)
	{
		Vector3 q = p.cwiseAbs() - Vector3(1, 1, 1);
		return q.cwiseMax(0.0f).norm() + std::min(q.maxCoeff(), 0.0f);
	}

	uint32_t GetSDFVersion()
	{
		return 1;
	}

	void OShapeGroup::AddedToScene(class OScene* InScene)
	{
		OElement::AddedToScene(InScene);

		if (!InScene) return;

		_GenerateShapes();

		auto thisRenderableScene = dynamic_cast<ORenderableScene*>(InScene);
		SE_ASSERT(thisRenderableScene);

		auto SceneType = InScene->get_type();
		if (!_shapeCache.empty() &&
			SceneType.is_derived_from(rttr::type::get<ORenderableScene>()))
		{
			// not ready yet
			if (!thisRenderableScene->GetGraphicsDevice()
				|| !thisRenderableScene->GetRenderScene()) return;

			GD_RenderableSignedDistanceField::Args sdfArgs;

			sdfArgs.position = _translation;
			sdfArgs.eulerRotationYPR = _rotation;
			sdfArgs.shapes = _shapeCache;
			sdfArgs.color = _color;
			
			auto sceneGD = thisRenderableScene->GetGraphicsDevice();
			_renderableSDF = sceneGD->CreateSignedDistanceField();
			_renderableSDF->SetSDFArgs(sdfArgs);
			_renderableSDF->AddToRenderScene(thisRenderableScene->GetRenderScene());
		}
	}


	void OShapeGroup::_GenerateShapes()
	{
		_shapeCache.clear();
		for (auto& curChild : _children)
		{
			SE_ASSERT(curChild);
			auto childType = curChild->get_type();
			SE_ASSERT(childType.is_derived_from(rttr::type::get<OShape>()));

			_shapeCache.push_back(((OShape*)curChild)->GenerateShape());
		}
	}

	void OShapeGroup::RemovedFromScene()
	{
		OElement::RemovedFromScene();

		if (_renderableSDF)
		{
			GPUThreaPool->enqueue([_renderableSDF = this->_renderableSDF]()
			{
				_renderableSDF->RemoveFromRenderScene();
			});
			_renderableSDF.reset();
		}
	}

	bool OShapeGroup::Intersect_Ray(const Ray& InRay, IntersectionInfo& oInfo) const
	{
		SE_ASSERT(_children.size() == _shapeCache.size());

		Vector3 ro = (InRay.GetOrigin() - _translation).cast<float>();
		Vector3 rd = InRay.GetDirection();
				
		std::vector<float> rayScalars;
		rayScalars.resize(_children.size());
		for (int32_t ChildIter = 0; ChildIter < _children.size(); ChildIter++)
		{
			Vector3 rayStart = (ToVector4(ro) * _shapeCache[ChildIter].invTransform).head<3>();
			Vector3 rayUnitEnd = (ToVector4(ro+rd) * _shapeCache[ChildIter].invTransform).head<3>();
			rayScalars[ChildIter] = 1.0f / (rayUnitEnd - rayStart).norm();
		}

		float t = 0; // current distance traveled along ray

		for (int32_t Iter = 0; Iter < 32; ++Iter)
		{
			Vector3 p = ro + rd * t;

			float d = 1e10;
			for (int32_t ChildIter = 0; ChildIter < _children.size(); ChildIter++)
			{
				SE_ASSERT(_children[ChildIter]);
				auto childShape = dynamic_cast<OShape*>(_children[ChildIter]);

				Vector3 localP = (ToVector4(p) * _shapeCache[ChildIter].invTransform).head<3>();
				float cD = 1e10;

				switch (childShape->GetShapeType())
				{
				case EShapeType::Sphere:
					cD = sdSphere(localP);
					break;
				case EShapeType::Box:
					cD = sdBox(localP);
					break;
				}

				cD *= rayScalars[ChildIter];

				switch (childShape->GetShapeOp())
				{
				case EShapeOp::Add:
					d = opUnion(cD, d);
					break;
				case EShapeOp::Intersect:
					d = opIntersection(cD, d);
					break;
				case EShapeOp::Subtract:
					d = opSubtraction(cD, d);
					break;
				}
			}

			if (d < 0.001f)
			{
				oInfo.location = p.cast<double>() + _translation;
				return true;
			}

			if( d > 10000 )
			{
				return false;
			}

			t += d;
		}

		return false;
	}
}

using namespace SPP;

RTTR_REGISTRATION
{	
	rttr::registration::enumeration<EShapeOp>("EShapeOp")
		(
			rttr::value("Add",		EShapeOp::Add),
			rttr::value("Subtract",   EShapeOp::Subtract),
			rttr::value("Intersect",	EShapeOp::Intersect)
		);

	rttr::registration::class_<OShape>("OShape")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_shapeOp", &OShape::_shapeOp)(rttr::policy::prop::as_reference_wrapper)
		.property("_shapeBlendFactor", &OShape::_shapeBlendFactor)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<OShapeGroup>("OShapeGroup")
		.constructor<const std::string&, SPPDirectory*>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_color", &OShapeGroup::_color)(rttr::policy::prop::as_reference_wrapper)		
		;
}
