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
		class GD_RenderScene* _parentScene = nullptr;
		OctreeLinkPtr _octreeLink = nullptr;
		
		float _radius = 1.0f;
		
		Vector3d _position;
		Vector3 _eulerRotationYPR;
		Vector3 _scale;

		Matrix4x4 _cachedRotationScale;

		bool _bSelected = false;

		virtual void _AddToScene(class GD_RenderScene* InScene);
		virtual void _RemoveFromScene();

	public:
		struct Args
		{
			Vector3d position;
			Vector3 eulerRotationYPR;
			Vector3 scale;
		};

		Renderable()
		{
			_position = Vector3d(0, 0, 0);
			_eulerRotationYPR = Vector3(0, 0, 0);
			_scale = Vector3(1, 1, 1);
		}

		Renderable(Args &&InArgs)
		{
			_position = InArgs.position;
			_eulerRotationYPR = InArgs.eulerRotationYPR;
			_scale = InArgs.scale;
		}

		virtual ~Renderable() {}

		void SetSelected(bool InSelect)
		{
			_bSelected = InSelect;
		}

		Vector3d &GetPosition()
		{
			return _position;
		}

		Vector3& GetRotation()
		{
			return _eulerRotationYPR;
		}

		Vector3& GetScale()
		{
			return _scale;
		}

		GPU_CALL AddToScene(class GD_RenderScene* InScene)
		{
			this->_AddToScene(InScene);
			co_return;
		}
		GPU_CALL RemoveFromScene()
		{
			this->_RemoveFromScene();
			co_return;
		}
		

		virtual void DrawDebug(std::vector< struct DebugVertex >& lines) { };
		virtual void Draw() { };		

		Matrix4x4 GenerateLocalToWorldMatrix() const
		{
			const float degToRad = 0.0174533f;

			Eigen::AngleAxisf yawAngle(_eulerRotationYPR[0] * degToRad, Vector3::UnitY());
			Eigen::AngleAxisf pitchAngle(_eulerRotationYPR[1] * degToRad, Vector3::UnitX());
			Eigen::AngleAxisf rollAngle(_eulerRotationYPR[2] * degToRad, Vector3::UnitZ());
			Eigen::Quaternion<float> q = rollAngle * yawAngle * pitchAngle;

			Matrix3x3 scaleMatrix = Matrix3x3::Identity();
			scaleMatrix(0, 0) = _scale[0];
			scaleMatrix(1, 1) = _scale[1];
			scaleMatrix(2, 2) = _scale[2];
			Matrix3x3 rotationMatrix = q.matrix();
			Matrix4x4 localToWorld = Matrix4x4::Identity();

			localToWorld.block<3, 3>(0, 0) = scaleMatrix * rotationMatrix;
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
			
	class SPP_GRAPHICS_API GD_RenderScene
	{
	protected:
		Camera _view;
		LooseOctree _octree;
		std::list<Renderable*> _renderables;

		bool _bRenderToBackBuffer = true;
		bool _bUseBBWithCustomColor = true;

		GPUReferencer< GPURenderTarget > _activeRTs[5];
		GPUReferencer< GPURenderTarget > _activeDepth;

		GPUReferencer< GPUTexture > _skyBox;

	public:
		GD_RenderScene() 
		{
			_view.Initialize(Vector3d(0, 0, 0), Vector3(0,0,0), 45.0f, 1.77f);
			_octree.Initialize(Vector3d(0, 0, 0), 50000, 3);
		}
		virtual ~GD_RenderScene() {}

		virtual void AddedToGraphicsDevice() {};

		void SetRenderToBackBuffer(bool bInRenderToBackBuffer)
		{
			_bRenderToBackBuffer = bInRenderToBackBuffer;
		}
		void SetUseBackBufferDepthWithCustomColor(bool bInUseBackBufferDepths)
		{
			_bUseBBWithCustomColor = bInUseBackBufferDepths;
		}
		void SetSkyBox(GPUReferencer< GPUTexture > InSkyBox)
		{
			_skyBox = InSkyBox;
		}

		template<typename... T>
		void SetColorTargets(T... args)
		{
			static_assert(sizeof...(args) <= 5, "TO MANY TARGETS SET");
			GPUReferencer< GPURenderTarget > setRTs[] = { args... };

			int32_t Iter = 0;
			for (Iter = 0; Iter < ARRAY_SIZE(setRTs); Iter++)
			{
				_activeRTs[Iter] = setRTs[Iter];
			}

			for (; Iter < ARRAY_SIZE(_activeRTs); Iter++)
			{
				_activeRTs[Iter].Reset();
			}
		}

		void SetDepthTarget(GPUReferencer< GPURenderTarget > InActiveDepth)
		{
			_activeDepth = InActiveDepth;
		}

		void UnsetAllRTs()
		{
			for (int32_t Iter = 0; Iter < ARRAY_SIZE(_activeRTs); Iter++)
			{
				_activeRTs[Iter].Reset();
			}
			_activeDepth.Reset();
		}

		virtual void AddToScene(Renderable *InRenderable)
		{
			SE_ASSERT(IsOnGPUThread());
			_renderables.push_back(InRenderable);
			_octree.AddElement(InRenderable);
		}

		virtual void RemoveFromScene(Renderable *InRenderable)
		{
			SE_ASSERT(IsOnGPUThread());
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

	enum class EShapeOp : uint32_t
	{
		Add = 0,
		Subtract,
		Intersect
	};

	enum class EShapeType : uint32_t
	{
		Unknown = 0,
		Sphere,
		Box
	};

	struct SPP_GRAPHICS_API SDFShape
	{
		EShapeType shapeType = EShapeType::Unknown;
		Vector3 translation = { 0,0,0 };

		EShapeOp shapeOp = EShapeOp::Add;
		Vector3 eulerRotation = { 0,0,0 };

		Vector4 shapeBlendAndScale = { 0,0,0,0 };
		Vector4 params = { 0,0,0,0 };
	};

	class SPP_GRAPHICS_API GD_RenderableSignedDistanceField : public Renderable
	{
	protected:
		std::vector< SDFShape > _shapes;
		Vector3 _color = { 0,0,0 };
		GPUReferencer< GPUShader > _customShader;

	public:
		struct Args : Renderable::Args
		{
			std::vector< SDFShape > shapes;
			Vector3 color;
		};

		GD_RenderableSignedDistanceField(Args&& InArgs) : Renderable((Renderable::Args)InArgs)
		{
			_shapes = InArgs.shapes;
			_color = InArgs.color;
		}

		GD_RenderableSignedDistanceField() : Renderable() {}

		std::vector< SDFShape >& GetShapes()
		{
			return _shapes;
		}
		Vector3& GetColor()
		{
			return _color;
		}
		void SetShader(GPUReferencer< GPUShader > InShader)
		{
			_customShader = InShader;
		}
	};
}