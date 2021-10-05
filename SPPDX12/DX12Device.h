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

#include "AMD/D3D12MemAlloc.h"

#include <string>

#include <locale>
#include <codecvt>

#include <vector>
#include <wrl.h>
#include <shellapi.h>
#include <stdexcept>
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
		HDT_ShapeInfos = 0,
		HDT_MeshInfos,
		HDT_MeshletVertices,
		HDT_MeshletResource,
		HDT_UniqueVertexIndices,
		HDT_PrimitiveIndices,
		HDT_Textures,
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

	class D3D12PipelineState : public PipelineState
	{
	protected:
		ComPtr<ID3D12PipelineState> _state;

	public:
		ID3D12PipelineState* GetState()
		{
			return _state.Get();
		}

		virtual void UploadToGpu() override { }

		virtual const char* GetName() const override
		{
			return "D3D12PipelineState";
		}

		void Initialize(EBlendState InBlendState,
			ERasterizerState InRasterizerState,
			EDepthState InDepthState,
			EDrawingTopology InTopology,
			GPUReferencer < GPUInputLayout > InLayout,
			GPUReferencer< GPUShader> InVS,
			GPUReferencer< GPUShader> InPS,

			GPUReferencer< GPUShader> InMS = nullptr,
			GPUReferencer< GPUShader> InAS = nullptr,
			GPUReferencer< GPUShader> InHS = nullptr,
			GPUReferencer< GPUShader> InDS = nullptr,

			GPUReferencer< GPUShader> InCS = nullptr);
	};

	GPUReferencer < D3D12PipelineState >  GetD3D12PipelineState(EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		EDrawingTopology InTopology,
		GPUReferencer< GPUInputLayout > InLayout,
		GPUReferencer< GPUShader> InVS,
		GPUReferencer< GPUShader> InPS,
		GPUReferencer< GPUShader> InMS,
		GPUReferencer< GPUShader> InAS,
		GPUReferencer< GPUShader> InHS,
		GPUReferencer< GPUShader> InDS,
		GPUReferencer< GPUShader> InCS);

	class D3D12CommandListWrapper
	{
	private:
		std::list< GPUReferencer< GPUResource > > _activeResources;
		ID3D12GraphicsCommandList6* _cmdList = nullptr;

	public:
		D3D12CommandListWrapper(ID3D12GraphicsCommandList6* InCmdList);

		void FrameComplete();

		//ideally avoid these
		void AddManualRef(GPUReferencer< GPUResource > InRef);
		void SetRootSignatureFromVerexShader(GPUReferencer< GPUShader >& InShader);
		void SetPipelineState(GPUReferencer< class D3D12PipelineState >& InPSO);
		void SetupSceneConstants(class D3D12RenderScene& InScene);
	};

	class DX12Device : public GraphicsDevice
	{
	private:
		static const UINT FrameCount = 2;

		// Pipeline objects.
		//D3DX12_VIEWPORT m_viewport;
		//D3DX12_RECT m_scissorRect;
		ComPtr<IDXGISwapChain3> _swapChain;
		ComPtr<ID3D12Device2> _device;
		ComPtr<IDXGIAdapter1> _hardwareAdapter;
		ComPtr<D3D12MA::Allocator> _allocator;
		D3D_FEATURE_LEVEL _featureLevel;

		GPUReferencer< GPURenderTarget > _renderTargets[FrameCount];
		GPUReferencer< GPURenderTarget > _depthStencil[FrameCount];

		std::unique_ptr<D3D12CommandListWrapper> _commandListWrappers[FrameCount];

		std::unique_ptr<FrameState> _frameStates[FrameCount];
				
		ComPtr<ID3D12CommandAllocator> _commandDirectAllocator[FrameCount];
		ComPtr<ID3D12CommandAllocator> _commandCopyAllocator;
		ComPtr<ID3D12CommandQueue> _commandQueue;
		ComPtr<ID3D12RootSignature> _emptyRootSignature;
		
		ComPtr<ID3D12GraphicsCommandList6> _commandList;
		ComPtr<ID3D12GraphicsCommandList6> _uplCommandList;

		std::array<D3D12SimpleDescriptorBlock, HDT_Num> _presetDescriptorHeaps;
		std::unique_ptr< D3D12SimpleDescriptorHeap >  _descriptorGlobalHeap;		
		D3D12WritableDescriptorBlock _dynamicDescriptorHeapBlock;

		std::unique_ptr< D3D12SimpleDescriptorHeap >  _dynamicSamplerDescriptorHeap;
		D3D12WritableDescriptorBlock _dynamicSamplerDescriptorHeapBlock;

		std::unique_ptr< D3D12MemoryFramedChunkBuffer  >  _perDrawMemory;

		// Synchronization objects.
		uint32_t _frameIndex = 0;
		HANDLE _fenceEvent = nullptr;
		ComPtr<ID3D12Fence> _fence;
		uint64_t _fenceValues[FrameCount] = {};

		int32_t _deviceWidth = 0;
		int32_t _deviceHeight = 0;


		HWND _hwnd = nullptr;

		void _CreateFrameResouces();

	public:
		DX12Device();

		D3D12WritableDescriptorBlock* GetDynamicDescriptorHeap()
		{
			return &_dynamicDescriptorHeapBlock;
		}
		D3D12WritableDescriptorBlock* GetDynamicSamplerHeap()
		{
			return &_dynamicSamplerDescriptorHeapBlock;
		}

		UINT64 GetFrameCount() const 
		{
			return _fenceValues[_frameIndex];
		}

		GPURenderTarget* GetScreenColor() 
		{
			return _renderTargets[_frameIndex].get();
		}

		GPURenderTarget* GetScreenDepth() 
		{
			return _depthStencil[_frameIndex].get();
		}

		D3D12MA::Allocator* GetResourceAllocator()
		{
			return _allocator.Get();
		}

		D3D12MemoryFramedChunkBuffer* GetPerFrameScratchMemory();
		ID3D12Device2* GetDevice();
		ID3D12GraphicsCommandList6* GetUploadCommandList();
		ID3D12GraphicsCommandList6* GetCommandList();
		ID3D12CommandQueue* GetCommandQueue();
		class D3D12CommandListWrapper* GetCommandListWrapper();

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
			return _deviceWidth;
		}
		virtual int32_t GetDeviceHeight() const
		{
			return _deviceHeight;
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