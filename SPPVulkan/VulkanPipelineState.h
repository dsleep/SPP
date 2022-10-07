// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPVulkan.h"
#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "vulkan/vulkan.h"

namespace SPP
{
	struct VulkanPipelineStateKey
	{
		EBlendState blendState = EBlendState::Disabled;
		ERasterizerState rasterizerState = ERasterizerState::BackFaceCull;
		EDepthState depthState = EDepthState::Enabled;
		EDrawingTopology topology = EDrawingTopology::TriangleList;
		EDepthOp depthOp = EDepthOp::Always;

		uintptr_t inputLayout = 0;

		uintptr_t vs = 0;
		uintptr_t ps = 0;
		uintptr_t ms = 0;
		uintptr_t as = 0;
		uintptr_t hs = 0;
		uintptr_t ds = 0;
		uintptr_t cs = 0;

		bool operator<(const VulkanPipelineStateKey& compareKey)const;
	};

	class VulkanPipelineState : public PipelineState
	{
	private:
		// The pipeline layout is used by a pipeline to access the descriptor sets
		// It defines interface (without binding any actual data) between the shader stages used by the pipeline and the shader resources
		// A pipeline layout can be shared among multiple pipelines as long as their interfaces match
		VkPipelineLayout _pipelineLayout = nullptr;

		// The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
		// Like the pipeline layout it's pretty much a blueprint and can be used with different descriptor sets as long as their layout matches
		std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;

		// Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
		// While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
		// So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
		// Even though this adds a new dimension of planing ahead, it's a great opportunity for performance optimizations by the driver
		VkPipeline _pipeline = nullptr;

		// for debugging or just leave in?!
		std::map<uint8_t, std::vector<VkDescriptorSetLayoutBinding> > _setLayoutBindings;

		virtual void _MakeResident() override {}
		virtual void _MakeUnresident() override {}

	public:
		VulkanPipelineState(GraphicsDevice* InOwner);
		virtual ~VulkanPipelineState();

		const VkPipeline &GetVkPipeline()
		{
			return _pipeline;
		}
		const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts()
		{
			return _descriptorSetLayouts;
		}
		const VkPipelineLayout &GetVkPipelineLayout()
		{
			return _pipelineLayout;
		}
		const std::map<uint8_t, std::vector<VkDescriptorSetLayoutBinding> >& GetDescriptorSetLayoutBindings()
		{
			return _setLayoutBindings;
		}

		class GPUReferencer<class SafeVkDescriptorSet> CreateDescriptorSet(uint8_t Idx, VkDescriptorPool InPool) const;

		virtual const char* GetName() const { return "VulkanPipelineState"; }

		void Initialize(struct VkFrameDataContainer& renderPassData,
			EBlendState InBlendState,
			ERasterizerState InRasterizerState,
			EDepthState InDepthState,
			EDrawingTopology InTopology,
			EDepthOp InDepthOp,

			GPUReferencer < GPUInputLayout > InLayout,

			GPUReferencer< GPUShader> InVS,
			GPUReferencer< GPUShader> InPS,

			GPUReferencer< GPUShader> InMS,
			GPUReferencer< GPUShader> InAS,
			GPUReferencer< GPUShader> InHS,
			GPUReferencer< GPUShader> InDS,

			GPUReferencer< GPUShader> InCS);
	};

	GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(GraphicsDevice* InOwner, 
		struct VkFrameDataContainer& renderPassData,
		EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		EDrawingTopology InTopology,
		EDepthOp InDepthOp,

		GPUReferencer< class VulkanInputLayout > InLayout,
		GPUReferencer< class VulkanShader > InVS,
		GPUReferencer< class VulkanShader > InPS,
		GPUReferencer< class VulkanShader > InMS,
		GPUReferencer< class VulkanShader > InAS,
		GPUReferencer< class VulkanShader > InHS,
		GPUReferencer< class VulkanShader > InDS,
		GPUReferencer< class VulkanShader > InCS);

	GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(GraphicsDevice* InOwner,
		GPUReferencer< VulkanShader > InCS);

	GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(GraphicsDevice* InOwner,
		struct VkFrameDataContainer& renderPassData,
		EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		EDrawingTopology InTopology,
		EDepthOp InDepthOp,

		GPUReferencer< VulkanInputLayout > InLayout,
		GPUReferencer< VulkanShader > InVS,
		GPUReferencer< VulkanShader > InPS);

}        // namespace vks
