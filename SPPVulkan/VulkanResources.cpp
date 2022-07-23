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
	SafeVkCommandBuffer::SafeVkCommandBuffer(GraphicsDevice* InOwner, const VkCommandBufferAllocateInfo& info) : GPUResource(InOwner)
	{
		auto vulkanDevice = dynamic_cast<VulkanGraphicsDevice*>(InOwner);
		VkDevice InDevice = vulkanDevice->GetDevice();

		SE_ASSERT(_cmdBuf == nullptr);
		_owningDevice = InDevice;
		_owningPool = info.commandPool;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(_owningDevice, &info, &_cmdBuf));
	}
	SafeVkCommandBuffer::~SafeVkCommandBuffer()
	{
		if (_cmdBuf)
		{
			vkFreeCommandBuffers(_owningDevice, _owningPool, 1, &_cmdBuf);
		}
	}

}   
