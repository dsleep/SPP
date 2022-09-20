// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"

namespace SPP
{
	class PBRDeferredLighting
	{
	protected:
		VulkanGraphicsDevice* _owningDevice = nullptr;
		VulkanRenderScene* _owningScene = nullptr;
		GPUReferencer<SafeVkSampler> _nearestSampler;
		GPUReferencer<SafeVkDescriptorSet> _gbufferTextureSet, _dummySet;
		
	public:
		PBRDeferredLighting(VulkanRenderScene* InScene);
		void Render(RT_RenderableLight& InLight);
	};
}