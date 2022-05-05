// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPGPUResources.h"

#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanTools.h"

namespace SPP
{
	class VulkanBuffer : public GPUBuffer
	{
	protected:

		VkBuffer _buffer = VK_NULL_HANDLE;
		VkDeviceMemory _memory = VK_NULL_HANDLE;

		VkDescriptorBufferInfo descriptor = {};
		VkDeviceSize _size = 0;
		VkDeviceSize _alignment = 0;

		/** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
		VkBufferUsageFlags _usageFlags;
		/** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
		VkMemoryPropertyFlags _memoryPropertyFlags;

	public:
		VulkanBuffer(std::shared_ptr< ArrayResource > InCpuData);

		virtual ~VulkanBuffer();

		virtual void UploadToGpu() override;
		virtual void UpdateDirtyRegion(uint32_t Idx, uint32_t Count) override;

		VkBuffer GetBuffer() const
		{
			SE_ASSERT(_buffer);
			return _buffer;
		}
	};

	GPUReferencer< GPUBuffer > Vulkan_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
}