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
//#include "d3dx12.h"
#include "../3rdParty/dxc/inc/dxcapi.h"

#include <string>

#include <locale>
#include <codecvt>

#include <vector>
#include <wrl.h>
#include <shellapi.h>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "DX12Utils.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG) || defined(DBG)
inline void DXSetName(ID3D12Object* pObject, LPCWSTR name)
{
	pObject->SetName(name);
}
#else
inline void DXSetName(ID3D12Object*, LPCWSTR)
{
}
#endif

namespace SPP
{
	enum HEAP_DESCRIPTOR_TABLES
	{
		HDT_MeshInfos = 0,
		HDT_MeshletVertices,
		HDT_MeshletResource,
		HDT_UniqueVertexIndices,
		HDT_PrimitiveIndices,
		HDT_PBRTextures,
		HDT_Dynamic,
		HDT_Num
	};

	struct DescriptorTableConfig
	{
		int32_t ID;
		int32_t Count;
	};

	#define MAX_MESH_ELEMENTS 1024
	#define MAX_TEXTURE_COUNT 2048
	#define DYNAMIC_MAX_COUNT 20 * 1024

	class FrameState
	{

	};

	class DX12Device : public GraphicsDevice
	{
	private:
		static const UINT FrameCount = 2;

		// Pipeline objects.
		//D3DX12_VIEWPORT m_viewport;
		//D3DX12_RECT m_scissorRect;
		ComPtr<IDXGISwapChain3> m_swapChain;
		ComPtr<ID3D12Device2> m_device;
		ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
		ComPtr<ID3D12Resource> m_depthStencil[FrameCount];

		std::unique_ptr<FrameState> _frameStates[FrameCount];
				
		ComPtr<ID3D12CommandAllocator> m_commandDirectAllocator[FrameCount];

		ComPtr<ID3D12CommandAllocator> m_commandCopyAllocator;

		ComPtr<ID3D12CommandAllocator> m_bundleAllocator;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<ID3D12RootSignature> _emptyRootSignature;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12DescriptorHeap> m_dsvHeap;


		ComPtr<ID3D12PipelineState> m_pipelineState;
		ComPtr<ID3D12GraphicsCommandList6> m_commandList;
		ComPtr<ID3D12GraphicsCommandList6> m_uplCommandList;
		ComPtr<ID3D12GraphicsCommandList6> m_bundle;

		ComPtr<ID3D12Resource> _constantBuffer[FrameCount];

		std::array<D3D12SimpleDescriptorBlock, HDT_Num> _presetDescriptorHeaps;
		std::unique_ptr< D3D12SimpleDescriptorHeap >  _descriptorGlobalHeap;		
		D3D12WritableDescriptorBlock _dynamicDescriptorHeapBlock;

		std::unique_ptr< D3D12SimpleDescriptorHeap >  _dynamicSamplerDescriptorHeap;
		D3D12WritableDescriptorBlock _dynamicSamplerDescriptorHeapBlock;

		std::unique_ptr< D3D12MemoryFramedChunkBuffer  >  _perDrawMemory;


		UINT m_rtvDescriptorSize = 0;
		UINT m_dsvDescriptorSize = 0;

		// Synchronization objects.
		UINT m_frameIndex = 0;
		HANDLE m_fenceEvent = nullptr;
		ComPtr<ID3D12Fence> m_fence;
		UINT64 m_fenceValues[FrameCount] = {};

		int32_t DeviceWidth = 0;
		int32_t DeviceHeight = 0;


		HWND dxWindow = nullptr;

		void _CreateFrameResouces();

	public:
		DX12Device();

		D3D12WritableDescriptorBlock& GetDynamicDescriptorHeap()
		{
			return _dynamicDescriptorHeapBlock;
		}
		D3D12WritableDescriptorBlock& GetDynamicSamplerHeap()
		{
			return _dynamicSamplerDescriptorHeapBlock;
		}

		UINT64 GetFrameCount() const 
		{
			return m_fenceValues[m_frameIndex];
		}

		D3D12MemoryFramedChunkBuffer* GetPerDrawScratchMemory();
		ID3D12Device2* GetDevice();
		ID3D12GraphicsCommandList6* GetUploadCommandList();
		ID3D12GraphicsCommandList6* GetCommandList();
		ID3D12CommandQueue* GetCommandQueue();

		D3D12SimpleDescriptorBlock& GetPresetDescriptorHeap(int32_t Idx)
		{
			return _presetDescriptorHeaps[Idx];
		}

		void GetHardwareAdapter(
			IDXGIFactory1* pFactory,
			IDXGIAdapter1** ppAdapter,
			bool requestHighPerformanceAdapter = true);

		virtual void Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow) override;
		virtual void ResizeBuffers(int32_t NewWidth, int32_t NewHeight) override;


		virtual int32_t GetDeviceWidth() const
		{
			return DeviceWidth;
		}
		virtual int32_t GetDeviceHeight() const
		{
			return DeviceHeight;
		}

		void WaitForGpu();

		void GPUDoneWithFrame(UINT64 frameIDX);
		void BeginResourceCopy();

		void EndResourceCopy();

		virtual void BeginFrame() override;
		virtual void EndFrame() override;

		virtual void MoveToNextFrame() override;
	};

	extern class DX12Device* GGraphicsDevice;
}