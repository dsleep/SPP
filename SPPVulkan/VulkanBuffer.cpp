// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "VulkanBuffer.h"
#include "VulkanDevice.h"


namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	VulkanBuffer::VulkanBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData) : GPUBuffer(InType, InCpuData)
	{ 
		SE_ASSERT(InCpuData);
		_size = InCpuData->GetTotalSize();

		switch (InType)
		{
		case GPUBufferType::Simple:
			_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			break;
		case GPUBufferType::Array:
			_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			break;
		case GPUBufferType::Vertex:
			_usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			break;
		case GPUBufferType::Index:
			_usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			break;
		}

		_usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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
		_alignment = memReqs.alignment;
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
		
		// Attach the memory to the buffer object
		VK_CHECK_RESULT(vkBindBufferMemory(GGlobalVulkanDevice, _buffer, _memory, 0));

		//
		UploadToGpu();
	}

	VulkanBuffer::~VulkanBuffer()
	{
		if (_buffer)
		{
			vkDestroyBuffer(GGlobalVulkanDevice, _buffer, nullptr);
			_buffer = nullptr;
		}
		
		if (_memory)
		{
			vkFreeMemory(GGlobalVulkanDevice, _memory, nullptr);
			_memory = nullptr;
		}
	}

	// Calculates the size required for constant buffer alignment
	template <typename T>
	T GetAlignedSize(T size, T inalignment = 256)
	{
		const T alignment = inalignment;
		const T alignedSize = (size + alignment - 1) & ~(alignment - 1);
		return alignedSize;
	}
	
	void VulkanBuffer::UploadToGpu()
	{
		auto& perFrameScratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		auto &cmdBuffer = GGlobalVulkanGI->GetCopyCommandBuffer();
		auto activeFrame = GGlobalVulkanGI->GetActiveFrame();

		auto WritableChunk = perFrameScratchBuffer.Write(GetData(), GetDataSize(), activeFrame);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = WritableChunk.offsetFromBase;
		copyRegion.dstOffset = 0; 
		copyRegion.size = _size;
		vkCmdCopyBuffer(cmdBuffer, WritableChunk.buffer, _buffer, 1, &copyRegion);
	}

	void VulkanBuffer::UpdateDirtyRegion(uint32_t Offset, uint32_t Count)
	{
		auto& perFrameScratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		auto& cmdBuffer = GGlobalVulkanGI->GetCopyCommandBuffer();
		auto activeFrame = GGlobalVulkanGI->GetActiveFrame();

		auto eleSize = _cpuLink->GetPerElementSize();
		auto startPos = Offset * eleSize;
		auto writeAmount = Count * eleSize;

		auto WritableChunk = perFrameScratchBuffer.Write(GetData() + startPos, writeAmount, activeFrame);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = WritableChunk.offsetFromBase;
		copyRegion.dstOffset = startPos;
		copyRegion.size = writeAmount;
		vkCmdCopyBuffer(cmdBuffer, WritableChunk.buffer, _buffer, 1, &copyRegion);
	}

	//TODO FIX UP THESE
	GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData)
	{
		return Make_GPU<VulkanBuffer>(InType, InCpuData);
	}

	//
	PerFrameStagingBuffer::Chunk::Chunk(uint32_t InSize, uint8_t currentFrameIdx) : size(InSize), frameIdx(currentFrameIdx)
	{
		frameIdx = currentFrameIdx;

		VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		// Create the buffer handle
		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo(usageFlags, size);
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateBuffer(GGlobalVulkanDevice, &bufferCreateInfo, nullptr, &buffer));

		// Create the memory backing up the buffer handle
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		vkGetBufferMemoryRequirements(GGlobalVulkanDevice, buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		// Find a memory type index that fits the properties of the buffer
		memAlloc.memoryTypeIndex = GGlobalVulkanGI->GetVKSVulkanDevice()->getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
		// If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
		VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
		VK_CHECK_RESULT(vkAllocateMemory(GGlobalVulkanDevice, &memAlloc, nullptr, &memory));
		// Attach the memory to the buffer object
		VK_CHECK_RESULT(vkBindBufferMemory(GGlobalVulkanDevice, buffer, memory, 0));

		VK_CHECK_RESULT(vkMapMemory(GGlobalVulkanDevice, memory, 0, size, 0, (void**)&CPUAddr));
	}

	PerFrameStagingBuffer::Chunk::~Chunk()
	{
		vkUnmapMemory(GGlobalVulkanDevice, memory);
		vkDestroyBuffer(GGlobalVulkanDevice, buffer, nullptr);
		vkFreeMemory(GGlobalVulkanDevice, memory, nullptr);
	}

	uint32_t PerFrameStagingBuffer::Chunk::SizeRemaining()
	{
		return (size - currentOffset);
	}

	VulkanBufferSlice PerFrameStagingBuffer::Chunk::GetWritable(uint32_t DesiredSize, uint8_t currentFrameIdx)
	{
		SE_ASSERT(DesiredSize <= SizeRemaining());
		SE_ASSERT(currentFrameIdx == frameIdx);

		auto initialOffset = currentOffset;
		currentOffset += DesiredSize;

		return VulkanBufferSlice{ buffer, DesiredSize, initialOffset, CPUAddr + initialOffset };
	}

	PerFrameStagingBuffer::PerFrameStagingBuffer(uint32_t InDefaultChunkSize) : DefaultChunkSize(InDefaultChunkSize)
	{

	}
	PerFrameStagingBuffer::~PerFrameStagingBuffer()
	{

	}

	VulkanBufferSlice PerFrameStagingBuffer::GetWritable(uint32_t DesiredSize, uint8_t FrameIdx)
	{
		SE_ASSERT(DesiredSize > 0);

		auto alignedSize = GetAlignedSize(DesiredSize);

		// just grab the last active if it works out
		if (!_chunks.empty())
		{
			auto currentChunk = _chunks.back();
			if (currentChunk->frameIdx == FrameIdx &&
				currentChunk->SizeRemaining() >= alignedSize)
			{
				return currentChunk->GetWritable(alignedSize, FrameIdx);
			}
		}

		for (auto it = _unboundChunks.begin(); it != _unboundChunks.end(); ++it)
		{
			auto currentChunk = *it;
			if (currentChunk->SizeRemaining() >= alignedSize)
			{
				// move to chunks
				_unboundChunks.erase(it);
				_chunks.push_back(currentChunk);

				currentChunk->frameIdx = FrameIdx;
				return currentChunk->GetWritable(alignedSize, FrameIdx);
			}
		}

		auto newChunkSize = GetAlignedSize<uint32_t>(alignedSize, DefaultChunkSize);
		auto newChunk = std::make_shared<Chunk>(newChunkSize, FrameIdx);
		_chunks.push_back(newChunk);

		return newChunk->GetWritable(alignedSize, FrameIdx);
	}

	VulkanBufferSlice PerFrameStagingBuffer::Write(const uint8_t* InData, uint32_t InSize, uint8_t FrameIdx)
	{
		auto memChunk = GetWritable(InSize, FrameIdx);
		memcpy(memChunk.cpuAddrWithOffset, InData, InSize);
		return memChunk;

	}

	void PerFrameStagingBuffer::FrameCompleted(uint8_t FrameIdx)
	{
		for (auto it = _chunks.begin(); it != _chunks.end();)
		{
			auto currentChunk = *it;
			if (currentChunk->frameIdx == FrameIdx)
			{
				// move to chunks
				it = _chunks.erase(it);

				currentChunk->frameIdx = std::numeric_limits<uint8_t >::max();
				currentChunk->currentOffset = 0;

				_unboundChunks.push_back(currentChunk);
			}
			else
			{
				it++;
			}
		}
	}
}