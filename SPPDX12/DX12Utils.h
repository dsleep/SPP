// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include "SPPCore.h"
#include "SPPString.h"
#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"
#include "SPPCamera.h"

#include <windows.h>

#include "d3dx12.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

namespace SPP
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		SE_ASSERT(SUCCEEDED(hr));
	}

	// Calculates the size required for constant buffer alignment
	template <typename T>
	T GetAlignedSize(T size, T inalignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
	{
		const T alignment = inalignment;
		const T alignedSize = (size + alignment - 1) & ~(alignment - 1);
		return alignedSize;
	}
	
	struct D3D12PartialResourceMemory
	{
		ComPtr<ID3D12Resource> orgResource;
		uint32_t size = 0;
		uint8_t* cpuAddr = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = 0;
		uint32_t offsetOrgResource = 0;
	};

	template<typename T>
	void WriteMem(D3D12PartialResourceMemory& memIn, uint32_t Offset, const T& data)
	{
		auto WriteSize = sizeof(T);
		SE_ASSERT(Offset + WriteSize <= memIn.size);

		static_assert(std::is_pod_v<T>, "Must be based on object");
		memcpy(memIn.cpuAddr + Offset, &data, WriteSize);
	}

	inline void WriteMem(D3D12PartialResourceMemory& memIn, const void *InData, size_t DataSize)
	{
		SE_ASSERT(DataSize <= memIn.size);
		memcpy(memIn.cpuAddr, InData, DataSize);
	}

	template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
	void WriteMem(D3D12PartialResourceMemory& memIn, uint32_t Offset, const Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols >& data)
	{
		auto WriteSize = sizeof(_Scalar) * _Rows * _Cols;
		SE_ASSERT(Offset + WriteSize <= memIn.size);
		using ThisMatrix = Eigen::Matrix< _Scalar, _Rows, _Cols, _Options, _MaxRows, _MaxCols >;
		memcpy(memIn.cpuAddr + Offset, (void*)data.data(), WriteSize);
	}

	class D3D12MemoryFramedChunkBuffer
	{
		struct Chunk
		{
			ComPtr<ID3D12Resource> resource;
			const uint32_t size = 0;
			UINT64 frameIdx = 0;
			uint8_t* CPUAddr = nullptr;
			uint32_t currentOffset = 0;

			Chunk(ID3D12Device* InDevice, uint32_t InSize, UINT64 currentFrameIdx);
			~Chunk();

			uint32_t SizeRemaining()
			{
				return (size - currentOffset);
			}

			D3D12PartialResourceMemory GetWritable(uint32_t DesiredSize, UINT64 currentFrameIdx);
		};

	private:		
		std::list< std::shared_ptr<Chunk> > _chunks;
		std::list< std::shared_ptr<Chunk> > _unboundChunks;
		ID3D12Device* _parentDevice;
		
	public:
		D3D12MemoryFramedChunkBuffer(ID3D12Device* InDevice);
		~D3D12MemoryFramedChunkBuffer();
		D3D12PartialResourceMemory GetWritable(uint32_t DesiredSize, UINT64 FrameIdx);
		D3D12PartialResourceMemory Write(const uint8_t* InData, uint32_t InSize, UINT64 FrameIdx);
		void FrameCompleted(UINT64 FrameIdx);
	};	

	struct D3D12SimpleDescriptor
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = { 0 };
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = { 0 };
	};

	struct D3D12SimpleDescriptorBlock : public D3D12SimpleDescriptor
	{
		uint32_t DescriptorSize = 0;
		uint32_t NumDescriptors = 0;

		D3D12SimpleDescriptor operator[](uint32_t index) const
		{
			SE_ASSERT(index < NumDescriptors);
			return 
			{
				D3D12_CPU_DESCRIPTOR_HANDLE{cpuHandle.ptr + index * (uint64_t)DescriptorSize},
				D3D12_GPU_DESCRIPTOR_HANDLE{gpuHandle.ptr + index * (uint64_t)DescriptorSize}
			};
		}

		D3D12SimpleDescriptorBlock GetBlock(uint32_t index, uint32_t Count) const
		{
			SE_ASSERT((index + Count) <= NumDescriptors);
			return
			{				
				D3D12_CPU_DESCRIPTOR_HANDLE{cpuHandle.ptr + index * (uint64_t)DescriptorSize},
				D3D12_GPU_DESCRIPTOR_HANDLE{gpuHandle.ptr + index * (uint64_t)DescriptorSize},
				DescriptorSize,
				Count
			};
		}
	};

	class D3D12SimpleDescriptorHeap
	{
	protected:
		ComPtr<ID3D12DescriptorHeap> _descriptorHeap;
		D3D12_DESCRIPTOR_HEAP_DESC _heapDesc = {};
		uint32_t _descriptorSize = 0;

	public:
		D3D12SimpleDescriptorHeap(ID3D12Device* InDevice, D3D12_DESCRIPTOR_HEAP_TYPE InType, int32_t InCount, bool bShaderVisible);

		ID3D12DescriptorHeap* GetDeviceHeap();

		uint32_t GetDescriptorCount() const;

		virtual ~D3D12SimpleDescriptorHeap();
		D3D12SimpleDescriptorBlock GetBlock(uint32_t index, uint32_t Count) const;
	};

	class D3D12WritableDescriptorBlock : public D3D12SimpleDescriptorBlock
	{
	protected:
		uint32_t _currentWriteIdx = 0;

	public:
		D3D12WritableDescriptorBlock() = default;
		D3D12WritableDescriptorBlock(const D3D12SimpleDescriptorBlock& InBlock)
		{
			*(D3D12SimpleDescriptorBlock*)this = InBlock;
		}
		D3D12SimpleDescriptorBlock GetDescriptorSlots(uint8_t SlotCount = 1);
		D3D12_GPU_DESCRIPTOR_HANDLE WriteToDescriptors(ID3D12Device* InDevice, D3D12SimpleDescriptorHeap& srcHeap, int32_t Count = 0);
	};

}