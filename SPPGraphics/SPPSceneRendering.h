// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"
#include "SPPCamera.h"
#include "SPPOctree.h"
#include "SPPBitSetArray.h"
#include <set>

namespace SPP
{	
	enum class DrawingType
	{
		Opaque,
		Transparent,
		PostEffect
	};

	enum class DrawingMovement
	{
		Static,
		Dynamic
	};

	enum class DrawingFilter
	{
		None,
		Masked
	};

	enum class DrawingData
	{
		StaticMesh,
		SkinnedMesh,
		Particle,
		SDF
	};

	struct DrawingInfo
	{
		DrawingType drawingType = DrawingType::Opaque;
		DrawingMovement drawingMovement = DrawingMovement::Static;
		DrawingFilter drawingFilter = DrawingFilter::None;
		DrawingData drawingData = DrawingData::StaticMesh;


		bool operator <(const DrawingInfo& InCompare) const
		{
			if (drawingType != InCompare.drawingType)
			{
				return drawingType < InCompare.drawingType;
			}
			else if (drawingMovement != InCompare.drawingMovement)
			{
				return drawingMovement < InCompare.drawingMovement;
			}
			else if (drawingFilter != InCompare.drawingFilter)
			{
				return drawingFilter < InCompare.drawingFilter;
			}

			return drawingData < InCompare.drawingData;
		}
	};

	class SPP_GRAPHICS_API GlobalRenderableID : public GPUResource
	{
	protected:
		uint32_t _globalID = 0;
		std::weak_ptr<RT_RenderScene> _scene;

	public:
		auto GetID() const { return _globalID; }
		GlobalRenderableID(GraphicsDevice* InOwner, std::shared_ptr<RT_RenderScene> currentScene);
		virtual ~GlobalRenderableID();
	};

	enum class RenderableType
	{
		Unknown = 0,
		Mesh = 1,
		Particle = 2,
		Light = 3
	};	

	class SPP_GRAPHICS_API Renderable : public IOctreeElement
	{
		friend class RT_RenderScene;

	protected:		
		GPUReferencer< GlobalRenderableID > _globalID;

		class RT_RenderScene* _parentScene = nullptr;
		OctreeLinkPtr _octreeLink = nullptr;
		
		Sphere _bounds;
		
		Vector3d _position;
		Vector3 _eulerRotation;
		Vector3 _scale;

		Matrix4x4 _cachedRotationScale;

		bool _bSelected = false;
		bool _bIsStatic = false;

		DrawingInfo _drawingInfo;

		virtual void _AddToRenderScene(class RT_RenderScene* InScene);
		virtual void _RemoveFromRenderScene();

	public:
		struct Args
		{
			Vector3d position = Vector3d(0, 0, 0);
			Vector3 eulerRotationYPR = Vector3(0, 0, 0);
			Vector3 scale = Vector3(1, 1, 1);
			bool bIsStatic = false;
			Sphere bounds;
		};

		Renderable()
		{
			_position = Vector3d(0, 0, 0);
			_eulerRotation = Vector3(0, 0, 0);
			_scale = Vector3(1, 1, 1);
		}

		void SetArgs(const Args& InArgs)
		{
			_position = InArgs.position;
			_eulerRotation = InArgs.eulerRotationYPR;
			_scale = InArgs.scale;
			_bIsStatic = InArgs.bIsStatic;
			_bounds = InArgs.bounds;
		}

		virtual ~Renderable() {}

		virtual RenderableType GetType() const { return RenderableType::Unknown; }

		virtual bool Is3dRenderable() const { return true; }
		virtual bool IsPostRenderable() const { return false; }
		bool IsStatic() const {
			return _bIsStatic;
		}
		const DrawingInfo &GetDrawingInfo() const
		{
			return _drawingInfo;
		}

		bool operator <(const Renderable& InCompare) const
		{
			return _drawingInfo < InCompare.GetDrawingInfo();;
		}

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

		auto& GetCachedRotationAndScale()
		{
			return _cachedRotationScale;
		}

		GPU_CALL AddToRenderScene(class RT_RenderScene* InScene)
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

		auto& GetGlobalID() const
		{
			return _globalID;
		}
		auto &GetSphereBounds() const
		{
			return _bounds;
		}

		virtual Spherei GetBounds() const
		{
			return Convert(_bounds);
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

	class SPP_GRAPHICS_API RT_RenderScene : public RT_Resource
	{
	protected:
		Camera _viewCPU;
		Camera _viewGPU;

		LooseOctree _octree;
		uint32_t _maxRenderableIdx = 0;
		std::vector<Renderable*> _renderables;

		BitSetArray _octreeVisiblity;
		BitSetArray _depthCullVisiblity;

		std::vector<Renderable*> _visible;
		std::vector<Renderable*> _visiblelights;

		std::vector<Renderable*> _opaques;
		std::vector<Renderable*> _translucents;

		//std::list<Renderable*> _renderablesPost;

		bool _bRenderToBackBuffer = true;
		bool _bUseBBWithCustomColor = true;

		GPUReferencer< GPURenderTarget > _activeRTs[5];
		GPUReferencer< GPURenderTarget > _activeDepth;
		GPUReferencer< GPUTexture > _skyBox;
		GPUReferencer< GPUTexture > _offscreenUI;

		std::list< uint32_t > _globalRenderableIDPool;
		uint32_t _globalRenderableIDCounter = 1;

	public:

		RT_RenderScene(GraphicsDevice* InOwner);
		virtual ~RT_RenderScene();

		auto& GetOctree()
		{
			return _octree;
		}


		uint32_t GetGlobalRenderableID()
		{
			if (_globalRenderableIDPool.size())
			{
				auto getBack = _globalRenderableIDPool.back();
				_globalRenderableIDPool.pop_back();
				return getBack;
			}
			return _globalRenderableIDCounter++;
		}
		void ReturnToGlobalRenderableID(uint32_t InID )
		{
			_globalRenderableIDPool.push_back(InID);
		}

		virtual void AddedToGraphicsDevice();
		virtual void RemovedFromGraphicsDevice();

		auto GetMaxRenderableIdx() const
		{
			return _maxRenderableIdx;
		}

		void SetRenderToBackBuffer(bool bInRenderToBackBuffer);
		void SetUseBackBufferDepthWithCustomColor(bool bInUseBackBufferDepths);
		void SetSkyBox(GPUReferencer< GPUTexture > InSkyBox);
		void PushUIUpdate(const Vector2i& FullSize, const Vector2i& Start, const Vector2i& Extents, const void* Memory, uint32_t MemorySize);

		virtual std::shared_ptr<RT_Material> GetDefaultMaterial() { return nullptr; }

		GraphicsDevice* GetOwner() const
		{
			return _owner;
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

		void SetDepthTarget(GPUReferencer< GPURenderTarget > InActiveDepth);
		void UnsetAllRTs();
		virtual void AddRenderable(Renderable* InRenderable);
		virtual void RemoveRenderable(Renderable* InRenderable);

		Camera& GetCPUCamera()
		{
			return _viewCPU;
		}

		Camera& GetGPUCamera()
		{
			return _viewGPU;
		}

		template<typename T>
		T& GetAs()
		{
			return *(T*)this;
		}

		auto& GetOctreeVisiblity()
		{
			return _octreeVisiblity;
		}

		auto& GetDepthCullVisiblity()
		{
			return _depthCullVisiblity;
		}

		virtual void PrepareScenesToDraw();

		virtual void BeginFrame();
		virtual void Draw();
		virtual void EndFrame();

		virtual void ResizeBuffers(int32_t NewWidth, int32_t NewHeight) {} 

		virtual void AddDebugLine(const Vector3d& Start, const Vector3d& End, const Vector3& Color = Vector3(1, 1, 1)) {}
		virtual void AddDebugBox(const Vector3d& Center, const Vector3d& Extents, const Vector3& Color = Vector3(1, 1, 1)) {}
		virtual void AddDebugSphere(const Vector3d& Center, float Radius, const Vector3& Color = Vector3(1, 1, 1)) {}
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
		Box,
		Cylinder
	};

	//_declspec(align(256u))
	struct SPP_GRAPHICS_API SDFShape
	{
		Matrix4x4 invTransform;

		Vector4 shapeParams;
		Vector3 shapeColor;

		EShapeType shapeType = EShapeType::Unknown;
		EShapeOp shapeOp = EShapeOp::Add;		
	};

	class SPP_GRAPHICS_API RT_RenderableSignedDistanceField : public Renderable, public RT_Resource
	{
	protected:
		std::vector< SDFShape > _shapes;
		Vector3 _color = { 0,0,0 };
		GPUReferencer< GPUShader > _customShader;

		RT_RenderableSignedDistanceField(GraphicsDevice* InOwner) : RT_Resource(InOwner) {}
	public:
		struct Args : Renderable::Args
		{
			std::vector< SDFShape > shapes;
			Vector3 color = { 0,0,0 };
		};

		void SetSDFArgs(const Args& InArgs)
		{
			SetArgs(InArgs);

			_shapes = InArgs.shapes;
			_color = InArgs.color;
		}

		virtual bool Is3dRenderable() const override { return false; }
		virtual bool IsPostRenderable() const override { return true; }

		virtual ~RT_RenderableSignedDistanceField() {}
		//RT_RenderableSignedDistanceField(Args&& InArgs) : Renderable((Renderable::Args)InArgs)
		//{
		//	_shapes = InArgs.shapes;
		//	_color = InArgs.color;
		//}

		//RT_RenderableSignedDistanceField() : Renderable() {}

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

	class SPP_GRAPHICS_API RT_StaticMesh : public RT_Resource
	{
	protected:
		EDrawingTopology _topology = EDrawingTopology::TriangleList;

		std::vector<VertexStream> _vertexStreams;
		std::shared_ptr<ArrayResource> _vertexResource;
		std::shared_ptr<ArrayResource> _indexResource;
		
		GPUReferencer<GPUInputLayout> _layout;

		std::shared_ptr<RT_Buffer> _vertexBuffer;
		std::shared_ptr<RT_Buffer> _indexBuffer;

		RT_StaticMesh(GraphicsDevice* InOwner);

	public:
		struct Args 
		{
			EDrawingTopology topology = EDrawingTopology::TriangleList;
			std::vector<VertexStream> vertexStreams;
			std::shared_ptr<ArrayResource> vertexResource;
			std::shared_ptr<ArrayResource> indexResource;
		};

		void SetMeshArgs(const Args& InArgs)
		{
			_topology = InArgs.topology;
			_vertexStreams = InArgs.vertexStreams;
			_vertexResource = InArgs.vertexResource;
			_indexResource = InArgs.indexResource;
		}

		virtual ~RT_StaticMesh() {}

		std::shared_ptr<RT_Buffer> GetVertexBuffer()
		{
			return _vertexBuffer;
		}
		std::shared_ptr<RT_Buffer> GetIndexBuffer()
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

	enum class ELightType
	{
		Unknown,
		Sun,
		Point
	};

	class SPP_GRAPHICS_API RT_RenderableLight : public Renderable, public RT_Resource
	{
		CLASS_RT_RESOURCE();

	protected:
		Vector3 _irradiance;
		RT_RenderableLight(GraphicsDevice* InOwner) : Renderable(), RT_Resource(InOwner) {}

	public:
		struct Args
		{
			Vector3 irradiance;
		};

		virtual RenderableType GetType() const override { return RenderableType::Light; }
		virtual ELightType GetLightType() const { return ELightType::Unknown; }

		void SetLightArgs(const Args& InArgs)
		{
			_irradiance = InArgs.irradiance;
		}
		auto& GetIrradiance()
		{
			return _irradiance;
		}
		virtual ~RT_RenderableLight() {}
	};

	class SPP_GRAPHICS_API RT_SunLight : public RT_RenderableLight
	{
		CLASS_RT_RESOURCE();

	protected:
		RT_SunLight(GraphicsDevice* InOwner) : RT_RenderableLight(InOwner) {}

	public:
		virtual ELightType GetLightType() const { return ELightType::Sun; }
		virtual ~RT_SunLight() {}
	};

	class SPP_GRAPHICS_API RT_PointLight : public RT_RenderableLight
	{
		CLASS_RT_RESOURCE();

	protected:
		RT_PointLight(GraphicsDevice* InOwner) : RT_RenderableLight(InOwner) {}

	public:
		virtual ELightType GetLightType() const { return ELightType::Point; }
		virtual ~RT_PointLight() {}
	};

	class SPP_GRAPHICS_API RT_RenderableMesh : public Renderable, public RT_Resource
	{
		CLASS_RT_RESOURCE();

	protected:
		std::shared_ptr<RT_StaticMesh> _mesh;
		std::shared_ptr<RT_Material> _material;

		RT_RenderableMesh(GraphicsDevice* InOwner) : Renderable(), RT_Resource(InOwner) {}

	public:
		struct Args
		{
			std::shared_ptr<RT_StaticMesh> mesh;
			std::shared_ptr<RT_Material> material;
		};

		std::shared_ptr<RT_Material> GetMaterial()
		{
			return _material;
		}

		std::shared_ptr<RT_StaticMesh> GetStaticMesh()
		{
			return _mesh;
		}

		virtual RenderableType GetType() const override { return RenderableType::Mesh; }

		virtual ~RT_RenderableMesh() {}
		void SetRenderableMeshArgs(const Args& InArgs)
		{
			_mesh = InArgs.mesh;
			_material = InArgs.material;
		}
	};

	static constexpr uint8_t const rtMAX_VOXEL_LEVELS = 15;

	class SPP_GRAPHICS_API RT_RenderableSVVO : public Renderable, public RT_Resource
	{
		CLASS_RT_RESOURCE();

	protected:

		std::shared_ptr<RT_Buffer> _sparseBuffer[rtMAX_VOXEL_LEVELS];

		RT_RenderableSVVO(GraphicsDevice* InOwner) : Renderable(), RT_Resource(InOwner) {}

	public:
		
		std::shared_ptr<RT_Buffer> GetBufferLevel(uint8_t InLevel)
		{
			return _sparseBuffer[InLevel];
		}

		virtual ~RT_RenderableSVVO() {}
	};
}