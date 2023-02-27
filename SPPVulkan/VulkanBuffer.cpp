// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "VulkanBuffer.h"
#include "VulkanDevice.h"

#include "VulkanMemoryAllocator/vk_mem_alloc.h"

namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	struct VulkanBuffer::PrivImpl
	{
		std::vector<VmaAllocation> allocations;
	};

	VulkanBuffer::VulkanBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData) : GPUBuffer( InType, InCpuData), _impl(new PrivImpl())
	{ 
		SE_ASSERT(InCpuData);
		_size = InCpuData->GetTotalSize();

		switch (InType)
		{
		case GPUBufferType::Simple:
			_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			break;
		case GPUBufferType::Sparse:
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
		_MakeResident();
	}

	VulkanBuffer::VulkanBuffer(GPUBufferType InType, size_t BufferSize, bool IsCPUMem) : GPUBuffer(InType, nullptr), _impl(new PrivImpl())
	{
		_size = BufferSize;

		switch (InType)
		{
		case GPUBufferType::Simple:
			_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			break;
		case GPUBufferType::Sparse:
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

		_usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		//if (IsCPUMem)
		//{
		//	_usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		//}
		_memoryPropertyFlags = IsCPUMem ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		// Create the buffer handle
		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo(_usageFlags, _size);
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (InType == GPUBufferType::Sparse)
		{
			bufferCreateInfo.flags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
		}

		VK_CHECK_RESULT(vkCreateBuffer(GGlobalVulkanDevice, &bufferCreateInfo, nullptr, &_buffer));

		// Create the memory backing up the buffer handle
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		vkGetBufferMemoryRequirements(GGlobalVulkanDevice, _buffer, &_memReq);
		memAlloc.allocationSize = _memReq.size;

		// for Sparse this is the page alignment as well
		_alignment = _memReq.alignment;

		if (InType != GPUBufferType::Sparse)
		{
			// Find a memory type index that fits the properties of the buffer
			memAlloc.memoryTypeIndex = GGlobalVulkanGI->GetVKSVulkanDevice()->getMemoryType(_memReq.memoryTypeBits, _memoryPropertyFlags);
			// If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
			VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
			VK_CHECK_RESULT(vkAllocateMemory(GGlobalVulkanDevice, &memAlloc, nullptr, &_memory));
			// Attach the memory to the buffer object
			VK_CHECK_RESULT(vkBindBufferMemory(GGlobalVulkanDevice, _buffer, _memory, 0));

			if (IsCPUMem)
			{
				VK_CHECK_RESULT(vkMapMemory(GGlobalVulkanDevice, _memory, 0, _size, 0, (void**)&_CPUAddr));
			}
		}
		else
		{
			auto totalPageCount = DivRoundUp(_memReq.size, _memReq.alignment);
			_impl->allocations.resize(totalPageCount,nullptr);			
		}
	}
	
	void VulkanBuffer::SetSparsePageMem(BufferPageData* InPages, uint32_t PageCount)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		std::vector<uint32_t> memSync;

		struct AllocAndPage
		{
			uint32_t pageIdx;
			VmaAllocation alloc;
		};

		std::vector<AllocAndPage> freeAllocations;
		std::vector<uint32_t> newAllocations;

		// link them up
		for (uint32_t Iter = 0; Iter < PageCount; Iter++)
		{
			auto& curPage = InPages[Iter];
			if ( curPage.Data )
			{
				// update
				if (_impl->allocations[curPage.PageIdx] == nullptr)
				{
					newAllocations.push_back(curPage.PageIdx);
				}
				memSync.push_back(Iter);
			}
			else
			{
				SE_ASSERT(_impl->allocations[curPage.PageIdx]);
				freeAllocations.push_back({ curPage.PageIdx, _impl->allocations[curPage.PageIdx] });
				_impl->allocations[curPage.PageIdx] = nullptr;
			}
		}

		//VmaAllocation_T
		std::vector<VmaAllocation> allocations;
		std::vector<VmaAllocationInfo> allocInfo;

		{
			allocations.resize(newAllocations.size(), nullptr);
			allocInfo.resize(newAllocations.size());

			VkMemoryRequirements pageMemReq = _memReq;
			pageMemReq.size = pageMemReq.alignment;
			auto results = vmaAllocateMemoryPages(GGlobalVulkanGI->GetVMAAllocator(),
				&pageMemReq,
				&allocCreateInfo,
				PageCount,
				allocations.data(),
				allocInfo.data());
			SE_ASSERT(results == VK_SUCCESS);
		}

		auto pageSize = _memReq.alignment;
		std::vector<VkSparseMemoryBind> binds{ newAllocations.size() + freeAllocations.size() };
		
		// bind the new allocations
		for (uint32_t i = 0; i < newAllocations.size(); ++i)
		{
			auto pageIdx = newAllocations[i];

			binds[i] = {};
			binds[i].resourceOffset = pageSize * pageIdx;
			binds[i].size = pageSize;
			binds[i].memory = allocInfo[i].deviceMemory; 
			binds[i].memoryOffset = allocInfo[i].offset;

			SE_ASSERT(_impl->allocations[newAllocations[i]] == nullptr);

			// this page is now set
			_impl->allocations[pageIdx] = allocations[i];
		}

		// these are the frees
		for (uint32_t i = newAllocations.size(), j = 0; i < binds.size(); ++i, ++j)
		{
			binds[i] = {};
			binds[i].resourceOffset = freeAllocations[j].pageIdx * pageSize;
			binds[i].size = pageSize;
			binds[i].memory = VK_NULL_HANDLE;
		}

		VkSparseBufferMemoryBindInfo bindInfo;
		bindInfo.buffer = _buffer;
		bindInfo.bindCount = binds.size();
		bindInfo.pBinds = binds.data();

		VkBindSparseInfo spareInfo = vks::initializers::bindSparseInfo();
		spareInfo.bufferBindCount = 1;
		spareInfo.pBufferBinds = &bindInfo;

		//VkSemaphore(between submits on GPU queues) or VkFence(to wait or poll for finish on the CPU)

		VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo();
		SafeVkFence tempFence(fenceCreateInfo);
		auto& currentFence = tempFence.Get();
		vkResetFences(GGlobalVulkanDevice, 1, &currentFence);
		vkQueueBindSparse(GGlobalVulkanGI->GetSparseQueue(), 1, &spareInfo, currentFence);
		vkWaitForFences(GGlobalVulkanDevice, 1, &currentFence, VK_TRUE, UINT64_MAX);

		auto& perFrameScratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		auto& cmdBuffer = GGlobalVulkanGI->GetCopyCommandBuffer();
		auto activeFrame = GGlobalVulkanGI->GetActiveFrame();

		auto WritableChunk = perFrameScratchBuffer.GetWritable(memSync.size() * pageSize, activeFrame);

		std::vector<VkBufferCopy> copyRegions{ memSync.size() };
		for(uint32_t Iter = 0; Iter < memSync.size(); Iter++)
		{
			auto& curPage = InPages[memSync[Iter]];
			SE_ASSERT(curPage.Data);

			auto PageOffset = (Iter * pageSize);
			memcpy(WritableChunk.cpuAddrWithOffset + PageOffset, curPage.Data, pageSize);

			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = WritableChunk.offsetFromBase + PageOffset;
			copyRegion.dstOffset = PageOffset;
			copyRegion.size = pageSize;
			copyRegions[Iter] = copyRegion;
		}

		vkCmdCopyBuffer(cmdBuffer, WritableChunk.buffer, _buffer, copyRegions.size(), copyRegions.data());
	}

	void VulkanBuffer::CopyTo(VkCommandBuffer cmdBuf, VulkanBuffer& DstBuf, size_t InCopySize)
	{
		size_t copySize = InCopySize ? InCopySize : std::min(_size, DstBuf._size);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = copySize;
		vkCmdCopyBuffer(cmdBuf, _buffer, DstBuf._buffer, 1, &copyRegion);
	}

	VulkanBuffer::~VulkanBuffer()
	{
		if (_CPUAddr)
		{
			vkUnmapMemory(GGlobalVulkanDevice, _memory);
		}

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
	
	void VulkanBuffer::_MakeResident()
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

		//VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
		//bufferBarrier.buffer = _buffer;
		//bufferBarrier.size = VK_WHOLE_SIZE;
		//bufferBarrier.srcAccessMask = VK_ACCESS_HOST_READ_BIT;
		//bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		//bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		//bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		//vkCmdPipelineBarrier(
		//	cmdBuffer,
		//	VK_PIPELINE_STAGE_TRANSFER_BIT,
		//	VK_PIPELINE_STAGE_HOST_BIT,
		//	VK_FLAGS_NONE,
		//	0, nullptr,
		//	1, &bufferBarrier,
		//	0, nullptr);


		//VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
		//bufferBarrier.buffer = _buffer;
		//bufferBarrier.size = VK_WHOLE_SIZE;
		//bufferBarrier.srcAccessMask = VK_ACCESS_HOST_READ_BIT;
		//bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		//bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		//bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	}

	void VulkanBuffer::UpdateDirtyRegion(uint32_t Offset, uint32_t Count)
	{
		auto& perFrameScratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		auto& cmdBuffer = GGlobalVulkanGI->GetCopyCommandBuffer();
		auto& graphicsCmdBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
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

		// copy barrier
		{
			VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
			bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.buffer = _buffer;
			bufferBarrier.size = VK_WHOLE_SIZE;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			vkCmdPipelineBarrier(
				cmdBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				1, &bufferBarrier,
				0, nullptr);
		}
		// graphics barrier
		{
			VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
			bufferBarrier.srcAccessMask = 0;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.buffer = _buffer;
			bufferBarrier.size = VK_WHOLE_SIZE;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			vkCmdPipelineBarrier(
				cmdBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_FLAGS_NONE,
				0, nullptr,
				1, &bufferBarrier,
				0, nullptr);
		}
	}

	//TODO FIX UP THESE
	GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData)
	{
		return Make_GPU(VulkanBuffer, InType, InCpuData);
	}


	//
	PerFrameStagingBuffer::Chunk::Chunk(uint32_t InSize, uint8_t currentFrameIdx) : size(InSize), frameIdx(currentFrameIdx)
	{
		frameIdx = currentFrameIdx;

		VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | 
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
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