// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "DX12Utils.h"

namespace SPP
{
	D3D12MemoryFramedChunkBuffer::Chunk::Chunk(ID3D12Device* InDevice, uint32_t InSize, UINT64 currentFrameIdx) : size(InSize)
	{		
		frameIdx = currentFrameIdx;

		auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto bufferResourceSize = CD3DX12_RESOURCE_DESC::Buffer(size);
				
		ThrowIfFailed(InDevice->CreateCommittedResource(
			&heapUpload,
			D3D12_HEAP_FLAG_NONE,
			&bufferResourceSize,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&resource)
		));

		ThrowIfFailed(resource->Map(0, nullptr, reinterpret_cast<void**>(&CPUAddr)));
	}

	D3D12MemoryFramedChunkBuffer::Chunk::~Chunk()
	{
		resource->Unmap(0, nullptr);
	}

	D3D12PartialResourceMemory D3D12MemoryFramedChunkBuffer::Chunk::GetWritable(uint32_t DesiredSize, UINT64 currentFrameIdx)
	{
		SE_ASSERT(DesiredSize <= SizeRemaining());
		SE_ASSERT(currentFrameIdx == frameIdx);

		auto initialOffset = currentOffset;
		currentOffset += DesiredSize;

		return D3D12PartialResourceMemory({ resource, DesiredSize, CPUAddr + initialOffset,  resource->GetGPUVirtualAddress() + initialOffset, initialOffset });
	}

	D3D12MemoryFramedChunkBuffer::D3D12MemoryFramedChunkBuffer(ID3D12Device* InDevice) : _parentDevice(InDevice)
	{		
	}

	D3D12MemoryFramedChunkBuffer::~D3D12MemoryFramedChunkBuffer()
	{
	}

	D3D12PartialResourceMemory D3D12MemoryFramedChunkBuffer::GetWritable(uint32_t DesiredSize, UINT64 FrameIdx)
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
		
		auto newChunkSize = GetAlignedSize<uint32_t> (alignedSize, 1 * 1024 * 1024);
		auto newChunk = std::make_shared<Chunk>(_parentDevice, newChunkSize, FrameIdx);
		_chunks.push_back(newChunk);

		return newChunk->GetWritable(alignedSize, FrameIdx);
	}


	D3D12PartialResourceMemory D3D12MemoryFramedChunkBuffer::Write(const uint8_t* InData, uint32_t InSize, UINT64 FrameIdx)
	{
		auto memChunk = GetWritable(InSize, FrameIdx);
		memcpy(memChunk.cpuAddr, InData, InSize);
		return memChunk;
	}

	void D3D12MemoryFramedChunkBuffer::FrameCompleted(UINT64 FrameIdx)
	{
		for (auto it = _chunks.begin(); it != _chunks.end();)
		{
			auto currentChunk = *it;
			if (currentChunk->frameIdx <= FrameIdx)
			{
				// move to chunks
				it = _chunks.erase(it);

				currentChunk->frameIdx = std::numeric_limits< UINT64 >::max();
				currentChunk->currentOffset = 0;

				_unboundChunks.push_back(currentChunk);
			}
			else
			{
				it++;
			}
		}
	}

	D3D12SimpleDescriptorHeap::D3D12SimpleDescriptorHeap(ID3D12Device* InDevice, D3D12_DESCRIPTOR_HEAP_TYPE InType, int32_t InCount, bool bShaderVisible)
	{
		_heapDesc.NumDescriptors = InCount;
		_heapDesc.Type = InType;
		_heapDesc.Flags = bShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(InDevice->CreateDescriptorHeap(&_heapDesc, IID_PPV_ARGS(&_descriptorHeap)));
		_descriptorSize = InDevice->GetDescriptorHandleIncrementSize(InType);
	}

	uint32_t D3D12SimpleDescriptorHeap::GetDescriptorCount() const
	{
		return _heapDesc.NumDescriptors;
	}

	ID3D12DescriptorHeap* D3D12SimpleDescriptorHeap::GetDeviceHeap()
	{
		return _descriptorHeap.Get();
	}

	D3D12SimpleDescriptorHeap::~D3D12SimpleDescriptorHeap()
	{
	}

	D3D12SimpleDescriptorBlock D3D12SimpleDescriptorHeap::GetBlock(uint32_t index, uint32_t Count) const
	{
		SE_ASSERT((index + Count) <= _heapDesc.NumDescriptors);
		return {
			D3D12_CPU_DESCRIPTOR_HANDLE{_descriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + index * (uint64_t)_descriptorSize},
			D3D12_GPU_DESCRIPTOR_HANDLE{_descriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + index * (uint64_t)_descriptorSize},
			_descriptorSize,
			Count
		};
	}


	D3D12SimpleDescriptorBlock D3D12WritableDescriptorBlock::GetDescriptorSlots(uint8_t SlotCount)
	{		
		if (_currentWriteIdx + SlotCount >= NumDescriptors)
		{
			_currentWriteIdx = 0;
		}
		auto CurrentBlock = GetBlock(_currentWriteIdx, SlotCount);
		_currentWriteIdx += SlotCount;
		return CurrentBlock;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE D3D12WritableDescriptorBlock::WriteToDescriptors(ID3D12Device* InDevice, D3D12SimpleDescriptorHeap& srcHeap, int32_t Count)
	{
		auto SlotCount = Count ? Count : srcHeap.GetDescriptorCount();
		auto CurrentDescriptorTable = GetDescriptorSlots(SlotCount);

		InDevice->CopyDescriptorsSimple(
			SlotCount,
			CurrentDescriptorTable.cpuHandle,
			srcHeap.GetDeviceHeap()->GetCPUDescriptorHandleForHeapStart(),
			//TODO THIS EVEN USEFUL ANYMORE?
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		return CurrentDescriptorTable.gpuHandle;
	}

}