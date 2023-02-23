// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#pragma once

#include "VulkanResources.h"
#include "VulkanDevice.h"

namespace SPP
{

	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	SafeVkCommandBuffer::SafeVkCommandBuffer(const VkCommandBufferAllocateInfo& info) : GPUResource()
	{
		SE_ASSERT(_cmdBuf == nullptr);
		_owningPool = info.commandPool;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(GGlobalVulkanDevice, &info, &_cmdBuf));
	}
	SafeVkCommandBuffer::~SafeVkCommandBuffer()
	{
		if (_cmdBuf)
		{
			vkFreeCommandBuffers(GGlobalVulkanDevice, _owningPool, 1, &_cmdBuf);
		}
	}

}   
