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
		VulkanGraphicsDevice* _owningDevice = nullptr;
		VulkanRenderScene* _owningScene = nullptr;
		GPUReferencer<SafeVkSampler> _nearestSampler;
		GPUReferencer<SafeVkDescriptorSet> _gbufferTextureSet, _dummySet, _shadowFilterDescriptorSet;
		
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