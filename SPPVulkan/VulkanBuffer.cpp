// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "VulkanBuffer.h"
#include "VulkanDevice.h"


namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;

	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	//VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT = 0x00000010,
	//VK_BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x00000020,
	//VK_BUFFER_USAGE_INDEX_BUFFER_BIT = 0x00000040,
	//VK_BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x00000080,

	//VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001,
	//VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x00000002,
	//VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x00000004,
	//VK_MEMORY_PROPERTY_HOST_CACHED_BIT = 0x00000008,

	VulkanBuffer::VulkanBuffer(std::shared_ptr< ArrayResource > InCpuData)
	{ 
		_size = InCpuData ? InCpuData->GetTotalSize() : 0;
		//_alignment = 0;

		_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		_memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		// Create the buffer handle
		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo(_usageFlags, _size);
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateBuffer(GGlobalVulkanDevice, &bufferCreateInfo, nullptr, &_buffer));

		// Create the memory backing up the buffer handle
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		vkGetBufferMemoryRequirements(GGlobalVulkanDevice, _buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		// Find a memory type index that fits the properties of the buffer
		memAlloc.memoryTypeIndex = GGlobalVulkanGI->GetVKSVulkanDevice()->getMemoryType(memReqs.memoryTypeBits, _memoryPropertyFlags);
		// If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
		VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
		if (_usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		{
			allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
			allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
			memAlloc.pNext = &allocFlagsInfo;
		}
		VK_CHECK_RESULT(vkAllocateMemory(GGlobalVulkanDevice, &memAlloc, nullptr, &_memory));

		if (InCpuData)
		{
			void* mapped = nullptr;
			VK_CHECK_RESULT(vkMapMemory(GGlobalVulkanDevice, _memory, 0, _size, 0, &mapped));
			memcpy(mapped, InCpuData->GetElementData(), InCpuData->GetTotalSize());
			// If host coherency hasn't been requested, do a manual flush to make writes visible
			if ((_memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			{
				VkMappedMemoryRange mappedRange = vks::initializers::mappedMemoryRange();
				mappedRange.memory = _memory;
				mappedRange.offset = 0;
				mappedRange.size = _size;
				vkFlushMappedMemoryRanges(GGlobalVulkanDevice, 1, &mappedRange);
			}
			vkUnmapMemory(GGlobalVulkanDevice, _memory);
		}

		// Attach the memory to the buffer object
		VK_CHECK_RESULT(vkBindBufferMemory(GGlobalVulkanDevice, _buffer, _memory, 0));

	}

	VulkanBuffer::~VulkanBuffer()
	{
		if (_buffer)
		{
			vkDestroyBuffer(GGlobalVulkanDevice, _buffer, nullptr);
			_buffer = nullptr;
		}
	}

	void VulkanBuffer::UploadToGpu()
	{

	}
	void VulkanBuffer::UpdateDirtyRegion(uint32_t Idx, uint32_t Count)
	{

	}

	//TODO FIX UP THESE
	GPUReferencer< GPUBuffer > Vulkan_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData)
	{
		switch (InType)
		{
		case GPUBufferType::Generic:
			return Make_GPU<VulkanBuffer>(InCpuData);
			break;
		case GPUBufferType::Index:
			break;
		case GPUBufferType::Vertex:
			break;
		case GPUBufferType::Global:
			break;
		}

		return nullptr;
	}
}