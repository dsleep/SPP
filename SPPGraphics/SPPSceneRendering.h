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
		Vector3 _eulerRotation;
		Vector3 _scale;

		Matrix4x4 _cachedRotationScale;

		bool _bSelected = false;
		bool _bIsStatic = false;

		virtual void _AddToRenderScene(class GD_RenderScene* InScene);
		virtual void _RemoveFromRenderScene();

	public:
		struct Args
		{
			Vector3d position;
			Vector3 eulerRotationYPR;
			Vector3 scale;
			bool bIsStatic = false;
		};

		Renderable()
		{
			_position = Vector3d(0, 0, 0);
			_eulerRotation = Vector3(0, 0, 0);
			_scale = Vector3(1, 1, 1);
		}

		Renderable(Args &&InArgs)
		{
			_position = InArgs.position;
			_eulerRotation = InArgs.eulerRotationYPR;
			_scale = InArgs.scale;
			_bIsStatic = InArgs.bIsStatic;
		}

		void SetArgs(const Args& InArgs)
		{
			_position = InArgs.position;
			_eulerRotation = InArgs.eulerRotationYPR;
			_scale = InArgs.scale;
			_bIsStatic = InArgs.bIsStatic;
		}

		virtual ~Renderable() {}

		virtual bool Is3dRenderable() const { return true; }
		virtual bool IsPostRenderable() const { return false; }

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
			return _eulerRotation;
		}

		Vector3& GetScale()
		{
			return _scale;
		}

		GPU_CALL AddToRenderScene(class GD_RenderScene* InScene)
		{
			this->_AddToRenderScene(InScene);
			co_return;
		}
		GPU_CALL RemoveFromRenderScene()
		{
			this->_RemoveFromRenderScene();
			co_return;
		}
		
		virtual void PrepareToDraw() {}
		virtual void DrawDebug(std::vector< struct DebugVertex >& lines) { };
		virtual void Draw() { };		

		Matrix4x4 GenerateLocalToWorldMatrix() const
		{
			Eigen::Quaternion<float> q = EulerAnglesToQuaternion(_eulerRotation);

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
			Eigen::Quaternion<float> q = EulerAnglesToQuaternion(_eulerRotation);

			Matrix3x3 scaleMatrix = Matrix3x3::Identity();
			scaleMatrix(0, 0) = _scale[0];
			scaleMatrix(1, 1) = _scale[1];
			scaleMatrix(2, 2) = _scale[2];

			Matrix3x3 rotationMatrix = q.matrix();

			return scaleMatrix * rotationMatrix;
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
		Camera _viewCPU;
		Camera _viewGPU;

		LooseOctree _octree;
		std::list<Renderable*> _renderables3d;

		std::list<Renderable*> _renderablesPost;

		bool _bRenderToBackBuffer = true;
		bool _bUseBBWithCustomColor = true;

		GPUReferencer< GPURenderTarget > _activeRTs[5];
		GPUReferencer< GPURenderTarget > _activeDepth;

		GPUReferencer< GPUTexture > _skyBox;

		GraphicsDevice* _owner = nullptr;

	public:

		GD_RenderScene(GraphicsDevice* InOwner) : _owner(InOwner)
		{
			_viewCPU.Initialize(Vector3d(0, 0, 0), Vector3(0,0,0), 45.0f, 1.77f);
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

		virtual std::shared_ptr<GD_Material> GetDefaultMaterial() { return nullptr; }

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

		virtual void AddRenderable(Renderable *InRenderable)
		{
			SE_ASSERT(IsOnGPUThread());

			if (InRenderable->Is3dRenderable())
			{
				_renderables3d.push_back(InRenderable);
			}

			if (InRenderable->IsPostRenderable())
			{
				_renderablesPost.push_back(InRenderable);
			}

			_octree.AddElement(InRenderable);
		}

		virtual void RemoveRenderable(Renderable *InRenderable)
		{
			SE_ASSERT(IsOnGPUThread());

			if (InRenderable->Is3dRenderable())
			{
				_renderables3d.remove(InRenderable);
			}

			if (InRenderable->IsPostRenderable())
			{
				_renderablesPost.remove(InRenderable);
			}

			_octree.RemoveElement(InRenderable);
		}

		Camera& GetCamera()
		{
			return _viewCPU;
		}

		template<typename T>
		T& GetAs()
		{
			return *(T*)this;
		}

		virtual void PrepareScenesToDraw()
		{ 
			_viewGPU = _viewCPU;
		};

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

	class SPP_GRAPHICS_API GD_RenderableSignedDistanceField : public Renderable, public GD_Resource
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

		void SetSDFArgs(const Args& InArgs)
		{
			_shapes = InArgs.shapes;
			_color = InArgs.color;
		}

		virtual bool Is3dRenderable() const override { return false; }
		virtual bool IsPostRenderable() const override { return true; }

		GD_RenderableSignedDistanceField(GraphicsDevice* InOwner) : GD_Resource(InOwner) {}
		virtual ~GD_RenderableSignedDistanceField() {}
		//GD_RenderableSignedDistanceField(Args&& InArgs) : Renderable((Renderable::Args)InArgs)
		//{
		//	_shapes = InArgs.shapes;
		//	_color = InArgs.color;
		//}

		//GD_RenderableSignedDistanceField() : Renderable() {}

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

	class SPP_GRAPHICS_API GD_StaticMesh : public GD_Resource
	{
	protected:
		EDrawingTopology _topology = EDrawingTopology::TriangleList;

		std::vector<VertexStream> _vertexStreams;
		std::shared_ptr<ArrayResource> _vertexResource;
		std::shared_ptr<ArrayResource> _indexResource;
		
		GPUReferencer<GPUInputLayout> _layout;

		std::shared_ptr<GD_Buffer> _vertexBuffer;
		std::shared_ptr<GD_Buffer> _indexBuffer;

	public:
		struct Args 
		{
			EDrawingTopology topology = EDrawingTopology::TriangleList;
			std::vector<VertexStream> vertexStreams;
			std::shared_ptr<ArrayResource> vertexResource;
			std::shared_ptr<ArrayResource> indexResource;
		};

		GD_StaticMesh(GraphicsDevice* InOwner);

		void SetMeshArgs(const Args& InArgs)
		{
			_topology = InArgs.topology;
			_vertexStreams = InArgs.vertexStreams;
			_vertexResource = InArgs.vertexResource;
			_indexResource = InArgs.indexResource;
		}

		virtual ~GD_StaticMesh() {}

		std::shared_ptr<GD_Buffer> GetVertexBuffer()
		{
			return _vertexBuffer;
		}
		std::shared_ptr<GD_Buffer> GetIndexBuffer()
		{
			return _indexBuffer;
		}
		EDrawingTopology GetTopology()
		{
			return _topology;
		}
		GPUReferencer<GPUInputLayout> GetLayout()
		{
			return _layout;
		}

		virtual void Initialize();
	};

	class SPP_GRAPHICS_API GD_RenderableMesh : public Renderable, public GD_Resource
	{
	protected:
		std::shared_ptr<GD_StaticMesh> _mesh;
		std::shared_ptr<GD_Material> _material;

	public:
		struct Args
		{
			std::shared_ptr<GD_StaticMesh> mesh;
			std::shared_ptr<GD_Material> material;
		};

		GD_RenderableMesh(GraphicsDevice* InOwner) : Renderable(), GD_Resource(InOwner) {}
		virtual ~GD_RenderableMesh() {}
		void SetRenderableMeshArgs(const Args& InArgs)
		{
			_mesh = InArgs.mesh;
			_material = InArgs.material;
		}
	};
}