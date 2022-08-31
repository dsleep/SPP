// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once


#include "vulkan/vulkan.h"

#include "VulkanDebugDrawing.h"
#include "SPPVulkan.h"
#include "SPPSceneRendering.h"

namespace SPP
{
	struct alignas(16u) GPUViewConstants
	{
		//all origin centered
		Matrix4x4 ViewMatrix;
		Matrix4x4 ViewProjectionMatrix;
		Matrix4x4 InvViewProjectionMatrix;
		Matrix4x4 InvProjectionMatrix;
		//real view position
		Vector3d ViewPosition;
		Vector4d FrustumPlanes[6];
		Vector2i FrameExtents;
		float RecipTanHalfFovy;
	};

	struct alignas(16u) GPUDrawConstants
	{
		//altered viewposition translated
		Matrix4x4 LocalToWorldScaleRotation;
		Vector3d Translation;
		uint32_t MaterialID;
	};

	struct alignas(16u) GPUDrawParams
	{
		//all origin centered
		Vector3 ShapeColor;
		uint32_t ShapeCount;
	};

	struct alignas(16u) GPUSDFShape
	{
		Vector3  translation;
		Vector3  eulerRotation;
		Vector4  shapeBlendAndScale;
		Vector4  params;
		uint32_t shapeType;
		uint32_t shapeOp;
	};

	struct alignas(16u) GPURenderableCullData
	{
		Vector3d center;
		float radius;
	};

	struct alignas(16u) GPUDrawCullData
	{
		float P00, P11, znear, zfar; // symmetric projection parameters
		float frustum[4]; // data for left/right/top/bottom frustum planes
		float pyramidWidth, pyramidHeight; // depth pyramid size in texels

		uint32_t drawCount;

		int32_t cullingEnabled;
		int32_t occlusionEnabled;
	};

	struct MaterialKey
	{
	private:
		size_t _hash = 0;

	public:
		std::weak_ptr<RT_Material> mat;
		uint32_t updateID = 0;

		MaterialKey(std::shared_ptr<RT_Material> InMat)
		{
			mat = InMat;
			updateID = InMat->GetUpdateID();
			_hash = (UINT_PTR)InMat.get() ^ updateID;
		}

		bool IsValid()
		{
			if (auto lckVal = mat.lock())
			{
				return (updateID == lckVal->GetUpdateID());
			}
			else
			{
				return false;
			}
		}

		size_t Hash() const
		{
			return _hash;
		}

		bool operator< (const MaterialKey& cmpTo) const
		{
			if (mat.owner_before(cmpTo.mat))
			{
				return updateID < cmpTo.updateID;
			}

			return false;
		}

		bool operator==(const MaterialKey& cmpTo) const
		{
			return !(*this < cmpTo) && !(cmpTo < *this);
		}
	};


	class VulkanRenderScene : public RT_RenderScene
	{
	protected:
		//D3D12PartialResourceMemory _currentFrameMem;
		////std::vector< D3D12RenderableMesh* > _renderMeshes;
		//bool _bMeshInstancesDirty = false;

		std::unique_ptr< class DepthDrawer > _depthDrawer;

		std::unique_ptr< class OpaqueDrawer > _opaqueDrawer;

		GPUReferencer< GPUShader > _debugVS;
		GPUReferencer< GPUShader > _debugPS;

		GPUReferencer< PipelineState > _debugPSO;
		GPUReferencer< GPUInputLayout > _debugLayout;

		std::shared_ptr < ArrayResource >  _debugResource;
		GPUReferencer< GPUBuffer > _debugBuffer;
		//std::vector< DebugVertex > _lines;

		GPUReferencer< GPUShader > _fullscreenRayVS;
		GPUReferencer< GPUShader > _fullscreenRaySDFPS, _fullscreenRaySkyBoxPS;

		GPUReferencer< PipelineState > _fullscreenRaySDFPSO, _fullscreenSkyBoxPSO;
		GPUReferencer< GPUInputLayout > _fullscreenRayVSLayout;

		
		std::shared_ptr< ArrayResource > _cameraData;
		GPUReferencer< class VulkanBuffer > _cameraBuffer;

		std::shared_ptr< ArrayResource > _renderableCullData;
		GPUReferencer< class VulkanBuffer > _renderableCullDataBuffer;

		GPUReferencer< class VulkanBuffer > _renderableVisibleGPU;
		GPUReferencer< class VulkanBuffer > _renderableVisibleCPU;
				
		std::shared_ptr<RT_Material> _defaultMaterial;

		std::unique_ptr<VulkanDebugDrawing> _debugDrawer;

		Planed _frustumPlanes[6];		

	public:
		VulkanRenderScene(GraphicsDevice* InOwner);
		virtual ~VulkanRenderScene();

		void DrawSkyBox();
		void WriteToFrame();

		GPUReferencer< GPUShader > GetSDFVS()
		{
			return _fullscreenRayVS;
		}
		GPUReferencer< GPUShader > GetSDFPS()
		{
			return _fullscreenRaySDFPS;
		}
		GPUReferencer< PipelineState > GetSDFPSO()
		{
			return _fullscreenRaySDFPSO;
		}
		GPUReferencer< GPUInputLayout > GetRayVSLayout()
		{
			return _fullscreenRayVSLayout;
		}
		GPUReferencer< class VulkanBuffer > GetCameraBuffer()
		{
			return _cameraBuffer;
		}

		virtual void AddedToGraphicsDevice() override;
		virtual void RemovedFromGraphicsDevice() override;		

		virtual std::shared_ptr<RT_Material> GetDefaultMaterial() override
		{
			return _defaultMaterial;
		}

		//void DrawDebug();

		//virtual void Build() {};

		//D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddrOfViewConstants()
		//{
		//	return _currentFrameMem.gpuAddr;
		//}

		virtual void AddRenderable(Renderable* InRenderable) override;
		virtual void RemoveRenderable(Renderable* InRenderable) override;
		
		virtual void BeginFrame() override;
		virtual void Draw() override;		
		virtual void EndFrame() override {}

		virtual void AddDebugLine(const Vector3d& Start, const Vector3d& End, const Vector3& Color = Vector3(1, 1, 1)) override;
		virtual void AddDebugBox(const Vector3d& Center, const Vector3d& Extents, const Vector3& Color = Vector3(1, 1, 1)) override;
		virtual void AddDebugSphere(const Vector3d& Center, float Radius, const Vector3& Color = Vector3(1, 1, 1)) override;


		void OpaqueDepthPass();
		void OpaquePass();
	};
}