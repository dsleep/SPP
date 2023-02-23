// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"
#include "VulkanFrameBuffer.hpp"

namespace SPP
{
	class PBRDeferredLighting
	{
	protected:
		VulkanRenderScene* _owningScene = nullptr;
		GPUReferencer<SafeVkSampler> _nearestSampler;
		GPUReferencer<SafeVkDescriptorSet> _gbufferTextureSet, _dummySet, _shadowFilterDescriptorSet, _sunDescSet;

		GPUReferencer<class VulkanTexture> _shadowDepthTexture, _shadowAttenuationTexture;
		GPUReferencer<class VulkanShader> _shadowFilterPS;
		GPUReferencer<class VulkanPipelineState> _shadowFilterPSO;

		std::unique_ptr<class VulkanFramebuffer> _shadowDepthFrameBuffer;
		


		VkFrameDataContainer _shadowRenderPass;

		std::unique_ptr<class VulkanFramebuffer> _shadowAttenuation;
		VkFrameDataContainer _shadowAttenuationRenderPass;		
		
	public:
		PBRDeferredLighting(VulkanRenderScene* InScene);
		void RenderSky();
		void Render(RT_RenderableLight& InLight);
		void RenderShadow(RT_RenderableLight& InLight);
	};
}