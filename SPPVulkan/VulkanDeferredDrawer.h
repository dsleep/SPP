// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"

namespace SPP
{
	class PBRDeferredDrawer
	{
	protected:
		GPUReferencer< SafeVkDescriptorSet > _camStaticBufferDescriptorSet;
		GPUReferencer< VulkanPipelineState > _voxelPBRPSO;
		GPUReferencer < VulkanBuffer > _emptyStorageBuffer;

		VulkanGraphicsDevice* _owningDevice = nullptr;
		VulkanRenderScene* _owningScene = nullptr;

	public:
		PBRDeferredDrawer(VulkanRenderScene* InScene);

		// TODO cleanupppp
		void Render(RT_VulkanRenderableMesh& InVulkanRenderableMesh);
		void RenderVoxelData(class RT_VulkanRenderableSVVO& InVoxelData);
	};
}