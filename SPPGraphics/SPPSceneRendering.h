// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"
#include "SPPCamera.h"
#include "SPPOctree.h"
#include <set>

namespace SPP
{	
	class SPP_GRAPHICS_API Renderable : public IOctreeElement
	{
	protected:
		class RenderScene* _parentScene = nullptr;
		OctreeLinkPtr _octreeLink = nullptr;
		
		float _radius = 1.0f;
		
		Vector3d _position;
		Vector3 _eulerRotationYPR;
		Vector3 _scale;

		Matrix4x4 _cachedRotationScale;

	public:
		Renderable()
		{
			_position = Vector3d(0, 0, 0);
			_eulerRotationYPR = Vector3(0, 0, 0);
			_scale = Vector3(1, 1, 1);
		}
		virtual ~Renderable() {}


		Vector3d &GetPosition()
		{
			return _position;
		}

		Vector3& GetScale()
		{
			return _scale;
		}

		virtual void AddToScene(class RenderScene* InScene);
		virtual void RemoveFromScene();

		virtual void DrawDebug(std::vector< class DebugVertex >& lines) { };
		virtual void Draw() { };		

		Matrix4x4 GenerateLocalToWorldMatrix() const
		{
			const float degToRad = 0.0174533f;

			Eigen::AngleAxisf yawAngle(_eulerRotationYPR[0] * degToRad, Vector3::UnitY());
			Eigen::AngleAxisf pitchAngle(_eulerRotationYPR[1] * degToRad, Vector3::UnitX());
			Eigen::AngleAxisf rollAngle(_eulerRotationYPR[2] * degToRad, Vector3::UnitZ());
			Eigen::Quaternion<float> q = rollAngle * yawAngle * pitchAngle;

			Matrix3x3 rotationMatrix = q.matrix();
			Matrix4x4 localToWorld = Matrix4x4::Identity();

			localToWorld(0, 0) *= _scale[0];
			localToWorld(1, 1) *= _scale[1];
			localToWorld(2, 2) *= _scale[2];

			localToWorld.block<1, 3>(3, 0) = Vector3((float)_position[0], (float)_position[1], (float)_position[2]);

			return localToWorld;
		}

		Matrix3x3 GenerateRotationScale() const
		{
			const float degToRad = 0.0174533f;

			Eigen::AngleAxisf yawAngle(_eulerRotationYPR[0] * degToRad, Vector3::UnitY());
			Eigen::AngleAxisf pitchAngle(_eulerRotationYPR[1] * degToRad, Vector3::UnitX());
			Eigen::AngleAxisf rollAngle(_eulerRotationYPR[2]  * degToRad, Vector3::UnitZ());
			Eigen::Quaternion<float> q = rollAngle * yawAngle * pitchAngle;

			Matrix3x3 rotationMatrix = q.matrix();
			rotationMatrix(0, 0) *= _scale[0];
			rotationMatrix(1, 1) *= _scale[1];
			rotationMatrix(2, 2) *= _scale[2];

			return rotationMatrix;
		}

		virtual Spherei GetBounds() const
		{
			return Convert(_position, _radius);
		}
		virtual void SetOctreeLink(OctreeLinkPtr InOctree)
		{
			_octreeLink = InOctree;
		}
		virtual const OctreeLinkPtr GetOctreeLink()
		{
			return _octreeLink;
		}
	};
			
	class SPP_GRAPHICS_API RenderScene
	{
	protected:
		Camera _view;
		LooseOctree _octree;
		std::list<Renderable*> _renderables;

	public:
		RenderScene() 
		{
			_view.Initialize(Vector3d(0, 0, 0), Vector3(0,0,0), 45.0f, 1.77f);
			_octree.Initialize(Vector3d(0, 0, 0), 50000, 3);
		}
		virtual ~RenderScene() {}

		virtual void AddToScene(Renderable *InRenderable)
		{
			_renderables.push_back(InRenderable);
			_octree.AddElement(InRenderable);
		}

		virtual void RemoveFromScene(Renderable *InRenderable)
		{
			_renderables.remove(InRenderable);
			_octree.RemoveElement(InRenderable);
		}

		Camera& GetCamera()
		{
			return _view;
		}

		template<typename T>
		T& GetAs()
		{
			return *(T*)this;
		}

		virtual void BeginFrame() { };
		virtual void Draw() { };
		virtual void EndFrame() { };
	};
	
	SPP_GRAPHICS_API std::shared_ptr<RenderScene> CreateRenderScene();
}