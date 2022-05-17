// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once


#include "vulkan/vulkan.h"

#include "SPPVulkan.h"
#include "SPPSceneRendering.h"

namespace SPP
{
	_declspec(align(256u)) struct GPUViewConstants
	{
		//all origin centered
		Matrix4x4 ViewMatrix;
		Matrix4x4 ViewProjectionMatrix;
		Matrix4x4 InvViewProjectionMatrix;
		//real view position
		Vector3d ViewPosition;
		Vector4d FrustumPlanes[6];
		float RecipTanHalfFovy;
	};

	_declspec(align(256u)) struct GPUDrawConstants
	{
		//altered viewposition translated
		Matrix4x4 LocalToWorldScaleRotation;
		Vector3d Translation;
	};

	_declspec(align(256u)) struct GPUDrawParams
	{
		//all origin centered
		Vector3 ShapeColor;
		uint32_t ShapeCount;
	};

	_declspec(align(256u)) struct GPUSDFShape
	{
		Vector3  translation;
		Vector3  eulerRotation;
		Vector4  shapeBlendAndScale;
		Vector4  params;
		uint32_t shapeType;
		uint32_t shapeOp;
	};

	class VulkanRenderScene : public GD_RenderScene
	{
	protected:
		//D3D12PartialResourceMemory _currentFrameMem;
		////std::vector< D3D12RenderableMesh* > _renderMeshes;
		//bool _bMeshInstancesDirty = false;

		GPUReferencer< GPUShader > _debugVS;
		GPUReferencer< GPUShader > _debugPS;

		GPUReferencer< PipelineState > _debugPSO;
		GPUReferencer< GPUInputLayout > _debugLayout;

		std::shared_ptr < ArrayResource >  _debugResource;
		GPUReferencer< GPUBuffer > _debugBuffer;
		std::vector< DebugVertex > _lines;

		GPUReferencer< GPUShader > _fullscreenRayVS;
		GPUReferencer< GPUShader > _fullscreenRaySDFPS, _fullscreenRaySkyBoxPS;

		GPUReferencer< PipelineState > _fullscreenRaySDFPSO, _fullscreenSkyBoxPSO;
		GPUReferencer< GPUInputLayout > _fullscreenRayVSLayout;

		
		std::shared_ptr< ArrayResource > _cameraData;
		GPUReferencer< class VulkanBuffer > _cameraBuffer;

		std::shared_ptr< ArrayResource > _drawConstants;
		GPUReferencer< class VulkanBuffer > _drawConstantsBuffer;

		std::shared_ptr< ArrayResource > _drawParams;
		GPUReferencer< class VulkanBuffer > _drawParamsBuffer;

		std::shared_ptr< ArrayResource > _shapes;
		GPUReferencer< class VulkanBuffer > _shapesBuffer;

		VkDescriptorSetLayout _perFrameSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _perDrawSetLayout = VK_NULL_HANDLE;

		VkDescriptorSet _perFrameDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet _perDrawDescriptorSet = VK_NULL_HANDLE;

		// Descriptor set pool
		VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;


	public:
		VulkanRenderScene();

		void DrawSkyBox();

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

		//void DrawDebug();

		//virtual void Build() {};

		//D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddrOfViewConstants()
		//{
		//	return _currentFrameMem.gpuAddr;
		//}

		virtual void AddToScene(Renderable* InRenderable) override;
		virtual void RemoveFromScene(Renderable* InRenderable) override;
		
		virtual void BeginFrame() override;
		virtual void Draw() override;		
		virtual void EndFrame() override {}
	};
}