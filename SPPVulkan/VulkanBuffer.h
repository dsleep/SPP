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
		VkDeviceSize _size = 0;
		VkDeviceSize _alignment = 0;
		VkMemoryRequirements _memReq = { 0 };

		uint8_t* _CPUAddr = nullptr;

		/** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
		VkBufferUsageFlags _usageFlags;
		/** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
		VkMemoryPropertyFlags _memoryPropertyFlags;

		virtual void _MakeResident() override; 
		virtual void _MakeUnresident() override {}

		std::vector<bool> sparsePages;

	public:
		VulkanBuffer(GraphicsDevice* InOwner, GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
		VulkanBuffer(GraphicsDevice* InOwner, GPUBufferType InType, size_t BufferSize, bool IsCPUMem);

		void CopyTo(VkCommandBuffer cmdBuf, VulkanBuffer& DstBuf, size_t InCopySize = 0);

		virtual ~VulkanBuffer();
		virtual void UpdateDirtyRegion(uint32_t Idx, uint32_t Count) override;

		VkDescriptorBufferInfo GetDescriptorInfo()
		{
			return VkDescriptorBufferInfo{
				.buffer = _buffer,
				.offset = 0,
				.range = _size
			};
		}

		auto GetMappedMemory()
		{
			return _CPUAddr;
		}

		VkBuffer &GetBuffer() 
		{
			SE_ASSERT(_buffer);
			return _buffer;
		}
	};

	struct VulkanBufferSlice
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		uint32_t size = 0;
		uint32_t offsetFromBase = 0;
		uint8_t* cpuAddrWithOffset = nullptr;
	};

	class PerFrameStagingBuffer
	{
		struct Chunk
		{
			VkBuffer buffer = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;

			const uint32_t size = 0;
			uint8_t frameIdx = 0;
			uint8_t* CPUAddr = nullptr;
			uint32_t currentOffset = 0;

			Chunk(uint32_t InSize, uint8_t currentFrameIdx); 
			~Chunk();
			uint32_t SizeRemaining();
			VulkanBufferSlice GetWritable(uint32_t DesiredSize, uint8_t currentFrameIdx);
		};

	private:
		std::list< std::shared_ptr<Chunk> > _chunks;
		std::list< std::shared_ptr<Chunk> > _unboundChunks;
		const uint32_t DefaultChunkSize;

	public:
		PerFrameStagingBuffer(uint32_t InDefaultChunkSize = 1 * 1024 * 1024);
		~PerFrameStagingBuffer();

		VulkanBufferSlice GetWritable(uint32_t DesiredSize, uint8_t FrameIdx);
		VulkanBufferSlice Write(const uint8_t* InData, uint32_t InSize, uint8_t FrameIdx);
		void FrameCompleted(uint8_t FrameIdx);
	};

	GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GraphicsDevice* InOwner, GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
}