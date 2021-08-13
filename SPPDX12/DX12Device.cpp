// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "DX12Device.h"

#include "DX12Shaders.h"
#include "DX12Buffers.h"
#include "DX12Textures.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"

#include <shlobj.h>

static std::wstring GetLatestWinPixGpuCapturerPath()
{
	LPWSTR programFilesPath = nullptr;
	SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

	stdfs::path pixInstallationPath = programFilesPath;
	pixInstallationPath /= "Microsoft PIX";

	std::wstring newestVersionFound;

	for (auto const& directory_entry : stdfs::directory_iterator(pixInstallationPath))
	{
		if (directory_entry.is_directory())
		{
			if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().c_str())
			{
				newestVersionFound = directory_entry.path().filename().c_str();
			}
		}
	}

	if (newestVersionFound.empty())
	{
		// TODO: Error, no PIX installation found
	}

	return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}

namespace SPP
{
	LogEntry LOG_D3D12Device("D3D12Device");

	extern bool ReadyMeshElement(std::shared_ptr<MeshElement> InMeshElement);

	// lazy externs
	extern std::shared_ptr< GPUShader > DX12_CreateShader(EShaderType InType);
	extern std::shared_ptr< GPUComputeDispatch> DX_12CreateComputeDispatch(std::shared_ptr< GPUShader> InCS);
	extern std::shared_ptr< GPUBuffer > DX12_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	extern std::shared_ptr< GPUInputLayout > DX12_CreateInputLayout();
	extern std::shared_ptr< GPURenderTarget > DX12_CreateRenderTarget();
	extern std::shared_ptr< GPUTexture > DX12_CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);

	static DescriptorTableConfig _tableRegions[] =
	{
		{ HDT_MeshInfos, 1 },
		{ HDT_MeshletVertices, MAX_MESH_ELEMENTS },
		{ HDT_MeshletResource, MAX_MESH_ELEMENTS },
		{ HDT_UniqueVertexIndices, MAX_MESH_ELEMENTS },
		{ HDT_PrimitiveIndices, MAX_MESH_ELEMENTS },

		{ HDT_PBRTextures, MAX_TEXTURE_COUNT },
		{ HDT_Dynamic, DYNAMIC_MAX_COUNT },
	};

	class DX12Device* GGraphicsDevice = nullptr;

	DX12Device::DX12Device()
	{
		GGraphicsDevice = this;
	}

	D3D12MemoryFramedChunkBuffer* DX12Device::GetPerDrawScratchMemory()
	{
		return _perDrawMemory.get();
	}

	ID3D12Device2* DX12Device::GetDevice()
	{
		return m_device.Get();
	}

	ID3D12GraphicsCommandList6* DX12Device::GetUploadCommandList()
	{
		return m_uplCommandList.Get();
	}
	ID3D12GraphicsCommandList6* DX12Device::GetCommandList()
	{
		return m_commandList.Get();
	}
	ID3D12CommandQueue* DX12Device::GetCommandQueue()
	{
		return m_commandQueue.Get();
	}

	void DX12Device::GetHardwareAdapter(
		IDXGIFactory1* pFactory,
		IDXGIAdapter1** ppAdapter,
		bool requestHighPerformanceAdapter)
	{
		*ppAdapter = nullptr;

		ComPtr<IDXGIAdapter1> adapter;
		ComPtr<IDXGIFactory6> factory6;
		if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
		{
			for (
				UINT adapterIndex = 0;
				DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
					adapterIndex,
					requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
					IID_PPV_ARGS(&adapter));
				++adapterIndex)
			{
				DXGI_ADAPTER_DESC1 desc;
				adapter->GetDesc1(&desc);

				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					// Don't select the Basic Render Driver adapter.
					// If you want a software adapter, pass in "/warp" on the command line.
					continue;
				}

				// Check to see whether the adapter supports Direct3D 12, but don't create the
				// actual device yet.
				if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
				{
					break;
				}
			}
		}
		else
		{
			for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
			{
				DXGI_ADAPTER_DESC1 desc;
				adapter->GetDesc1(&desc);

				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					// Don't select the Basic Render Driver adapter.
					// If you want a software adapter, pass in "/warp" on the command line.
					continue;
				}

				// Check to see whether the adapter supports Direct3D 12, but don't create the
				// actual device yet.
				if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
				{
					break;
				}
			}
		}

		*ppAdapter = adapter.Detach();
	}

	void DX12Device::Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow)
	{
		SPP_LOG(LOG_D3D12Device, LOG_INFO, "DX12Device::Initialize %d by %d", InitialWidth, InitialHeight);

		UINT dxgiFactoryFlags = 0;

		dxWindow = (HWND)OSWindow;

		DeviceWidth = InitialWidth;
		DeviceHeight = InitialHeight;

#if defined(_DEBUG)
		// Check to see if a copy of WinPixGpuCapturer.dll has already been injected into the application.
		// This may happen if the application is launched through the PIX UI. 

		//this allows pix attachments
#if 0
		if (GetModuleHandle(L"WinPixGpuCapturer.dll") == 0)
		{
			LoadLibrary(GetLatestWinPixGpuCapturerPath().c_str());
		}
#endif

		// Enable the debug layer (requires the Graphics Tools "optional feature").
		// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();

				// Enable additional debug layers.
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;


				ComPtr<ID3D12Debug1> spDebugController1;
				debugController->QueryInterface(IID_PPV_ARGS(&spDebugController1));
				//spDebugController1->SetEnableGPUBasedValidation(true);
			}
		}
#endif

		ComPtr<IDXGIFactory4> factory;
		ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

		const bool _useWarpDevice = false;

		//WARP is a high speed, fully conformant software rasterizer.It is a component of the DirectX graphics technology that was introduced by the Direct3D 11 runtime.
		if (_useWarpDevice)
		{
		    ComPtr<IDXGIAdapter> warpAdapter;
		    ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
		    ThrowIfFailed(D3D12CreateDevice(
		        warpAdapter.Get(),
		        D3D_FEATURE_LEVEL_11_0,
		        IID_PPV_ARGS(&m_device)
		    ));
		}
		else
		{
			ComPtr<IDXGIAdapter1> hardwareAdapter;
			GetHardwareAdapter(factory.Get(), &hardwareAdapter);

			ThrowIfFailed(D3D12CreateDevice(
				hardwareAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&m_device)
			));
		}

		D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
		m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
		if (options.TiledResourcesTier == D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED)
		{
			OutputDebugStringA("NO TILING SUPPORT\n");
		}
		if (options.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_2)
		{
			OutputDebugStringA("NO Bindless SUPPORT\n");
		}

		{
			// Check the maximum feature level, and make sure it's above our minimum
			D3D_FEATURE_LEVEL featureLevelsArray[4];
			featureLevelsArray[0] = D3D_FEATURE_LEVEL_11_0;
			featureLevelsArray[1] = D3D_FEATURE_LEVEL_11_1;
			featureLevelsArray[2] = D3D_FEATURE_LEVEL_12_0;
			featureLevelsArray[3] = D3D_FEATURE_LEVEL_12_1;
			D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels = { };
			featureLevels.NumFeatureLevels = (UINT)SPP::ARRAY_SIZE(featureLevelsArray);
			featureLevels.pFeatureLevelsRequested = featureLevelsArray;
			ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels)));
			//FeatureLevel = featureLevels.MaxSupportedFeatureLevel;
		}

		D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_5 };
		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))
			|| (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_5))
		{
			OutputDebugStringA("ERROR: Shader Model 6.5 is not supported\n");
			throw std::exception("Shader Model 6.5 is not supported");
		}

		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS7 featureData = {};
			m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &featureData, sizeof(featureData));

			if (featureData.MeshShaderTier == D3D12_MESH_SHADER_TIER_NOT_SUPPORTED)
			{
				//Supported Mesh Shader Use 
				OutputDebugStringA("NO MESH SHADER SUPPORT\n");
			}
			if (featureData.SamplerFeedbackTier == D3D12_SAMPLER_FEEDBACK_TIER_NOT_SUPPORTED)
			{
				//Supported SamplerFeedbackTier
				OutputDebugStringA("NO SAMPLER FEEDBACK SUPPORT\n");
			}
		}

		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

			// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

			if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}
		}
				
		// Describe and create the command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

		// Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.Width = DeviceWidth;
		swapChainDesc.Height = DeviceHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swapChain;
		ThrowIfFailed(factory->CreateSwapChainForHwnd(
			m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
			(HWND)OSWindow,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain
		));

		// This sample does not support fullscreen transitions.
		ThrowIfFailed(factory->MakeWindowAssociation((HWND)OSWindow, DXGI_MWA_NO_ALT_ENTER));

		ThrowIfFailed(swapChain.As(&m_swapChain));
		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

		// Create descriptor heaps.
		{
			// Describe and create a render target view (RTV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = FrameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

			// Describe and create a depth stencil view (DSV) descriptor heap.
			// Each frame has its own depth stencils (to write shadows onto) 
			// and then there is one for the scene itself.
			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = FrameCount;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));			

			int32_t TotalCount = 0;
			for (int32_t Iter = 0; Iter < ARRAY_SIZE(_tableRegions); Iter++)
			{
				TotalCount += _tableRegions[Iter].Count;
			}
			_descriptorGlobalHeap = std::make_unique< D3D12SimpleDescriptorHeap >(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, TotalCount, true);

			DXSetName(_descriptorGlobalHeap->GetDeviceHeap(), L"Global CBV/SRV/UAV Heap");

			int32_t CurrentStartIdx = 0;
			for (int32_t Iter = 0; Iter < ARRAY_SIZE(_tableRegions); Iter++)
			{				
				_presetDescriptorHeaps[Iter] = _descriptorGlobalHeap->GetBlock(CurrentStartIdx, _tableRegions[Iter].Count);
				CurrentStartIdx += _tableRegions[Iter].Count;
			}
			_dynamicDescriptorHeapBlock = _presetDescriptorHeaps[HDT_Dynamic];
			
			// dynamic sampler
			_dynamicSamplerDescriptorHeap = std::make_unique< D3D12SimpleDescriptorHeap >(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1 * 1024, true);
			_dynamicSamplerDescriptorHeapBlock = _dynamicSamplerDescriptorHeap->GetBlock(0, _dynamicSamplerDescriptorHeap->GetDescriptorCount());
			DXSetName(_dynamicSamplerDescriptorHeap->GetDeviceHeap(), L"Dyanmic Sampler Heap");

			_perDrawMemory = std::make_unique< D3D12MemoryFramedChunkBuffer >(m_device.Get());
			//DXSetName(_perDrawMemory->GetResource(), L"Per Draw Memory Chunk");

			m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

		// Create frame resources.
		_CreateFrameResouces();

		// direct allocators
		const UINT64 constantBufferSize = 5 * 1024 * 1024;
		auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto bufferResourceSize = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandDirectAllocator[n])));

			ThrowIfFailed(m_device->CreateCommittedResource(
				&heapUpload,
				D3D12_HEAP_FLAG_NONE,
				&bufferResourceSize,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&_constantBuffer[n])
			));
		}

		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandCopyAllocator)));

		/*
		The Root Signature is the object that represents the link between the command list and the resources used by the pipeline..

		It specifies the data types that shaders should expect from the application,
		and also which pipeline state objects are compatible (those compiled with the same layout) for the next draw/dispatch calls.
		*/

		// Create an empty root signature.
		{
			D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;

			rootSignatureDesc.NumParameters = 0;
			rootSignatureDesc.pParameters = nullptr;
			rootSignatureDesc.NumStaticSamplers = 0;
			rootSignatureDesc.pStaticSamplers = nullptr;
			rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
			ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_emptyRootSignature)));
		}

		// Create the command lists
		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandDirectAllocator[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandCopyAllocator.Get(), nullptr, IID_PPV_ARGS(&m_uplCommandList)));

		DXSetName(m_commandList.Get(), L"Default Command List");
		DXSetName(m_uplCommandList.Get(), L"Upload Command List");


		//// Command lists are created in the recording state, but there is nothing
		//// to record yet. The main loop expects it to be closed, so close it now.
		ThrowIfFailed(m_commandList->Close());
		ThrowIfFailed(m_uplCommandList->Close());


		// Create synchronization objects and wait until assets have been uploaded to the GPU.
		{
			ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
			m_fenceValues[m_frameIndex]++;

			// Create an event handle to use for frame synchronization.
			m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (m_fenceEvent == nullptr)
			{
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}

			WaitForGpu();
		}
	}

	void DX12Device::ResizeBuffers(int32_t NewWidth, int32_t NewHeight)
	{
		if (DeviceWidth != NewWidth || DeviceHeight != NewHeight)
		{
			SPP_LOG(LOG_D3D12Device, LOG_INFO, "DX12Device::ResizeBuffer %d by %d", NewWidth, NewHeight);

			WaitForGpu();

			// this needed ?, bascially get to 0 before reset
			while (m_frameIndex != 0)
			{
				BeginFrame();
				EndFrame();
				MoveToNextFrame();
			}

			for (UINT n = 0; n < FrameCount; n++)
			{
				m_renderTargets[n].Reset();
				m_depthStencil[n].Reset();
			}

			DeviceWidth = NewWidth;
			DeviceHeight = NewHeight;
			
			m_swapChain->ResizeBuffers(FrameCount, DeviceWidth, DeviceHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
			
			_CreateFrameResouces();
		}
	}

	void DX12Device::_CreateFrameResouces()
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE  rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE  dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < FrameCount; n++)
		{
			// Create the color surface
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);

			// Create the depth stencil view.
			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
			depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;// DXGI_FORMAT_D32_FLOAT;
			depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
			depthOptimizedClearValue.Format = depthStencilDesc.Format;
			depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			depthOptimizedClearValue.DepthStencil.Stencil = 0;

			auto heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			auto tex2DDesc = CD3DX12_RESOURCE_DESC::Tex2D(depthStencilDesc.Format, DeviceWidth, DeviceHeight, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
			ThrowIfFailed(m_device->CreateCommittedResource(
				&heapDefault,
				D3D12_HEAP_FLAG_NONE,
				&tex2DDesc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&depthOptimizedClearValue,
				IID_PPV_ARGS(&m_depthStencil[n])
			));

			//NAME_D3D12_OBJECT(m_depthStencil);

			m_device->CreateDepthStencilView(m_depthStencil[n].Get(), &depthStencilDesc, dsvHandle);
			dsvHandle.Offset(1, m_dsvDescriptorSize);
		}
	}

	void DX12Device::WaitForGpu()
	{
		// Schedule a Signal command in the queue.
		ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

		// Wait until the fence has been processed.
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

		// Increment the fence value for the current frame.
		m_fenceValues[m_frameIndex]++;
	}

	void DX12Device::BeginResourceCopy()
	{
		// Command list allocators can only be reset when the associated 
		// command lists have finished execution on the GPU; apps should use 
		// fences to determine GPU execution progress.
		ThrowIfFailed(m_commandCopyAllocator->Reset());
		ThrowIfFailed(m_uplCommandList->Reset(m_commandCopyAllocator.Get(), nullptr));
	}

	void DX12Device::EndResourceCopy()
	{
		// Close the command list and execute it to begin the initial GPU setup.
		ThrowIfFailed(m_uplCommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_uplCommandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		WaitForGpu();
	}

	extern void UpdateGPUMeshes();

	void DX12Device::BeginFrame()
	{		
		// Command list allocators can only be reset when the associated 
		// command lists have finished execution on the GPU; apps should use 
		// fences to determine GPU execution progress.
		ThrowIfFailed(m_commandDirectAllocator[m_frameIndex]->Reset());

		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		ThrowIfFailed(m_commandList->Reset(m_commandDirectAllocator[m_frameIndex].Get(), nullptr));

		// Set necessary state.
		m_commandList->SetGraphicsRootSignature(_emptyRootSignature.Get());
				
		ID3D12DescriptorHeap* ppHeaps[] = { 
			_descriptorGlobalHeap->GetDeviceHeap(),
			_dynamicSamplerDescriptorHeap->GetDeviceHeap()
		};
		m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		UpdateGPUMeshes();

		// Indicate that the back buffer will be used as a render target.
		auto ToRenderTarget(CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		m_commandList->ResourceBarrier(1, &ToRenderTarget);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_dsvDescriptorSize);
		m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		// Record commands.
		const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}

	void DX12Device::EndFrame()
	{
		// Indicate that the back buffer will now be used to present.
		auto ToPreset(CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		m_commandList->ResourceBarrier(1, &ToPreset);

		ThrowIfFailed(m_commandList->Close());

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Present the frame.
		ThrowIfFailed(m_swapChain->Present(1, 0));

		//
		MoveToNextFrame();
	}

	void DX12Device::GPUDoneWithFrame(UINT64 frameIDX)
	{
		_perDrawMemory->FrameCompleted(frameIDX);
	}

	void DX12Device::MoveToNextFrame()
	{
		// Schedule a Signal command in the queue.
		const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
		ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

		// Update the frame index.
		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

		// If the next frame is not ready to be rendered yet, wait until it is ready.
		if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
		{
			ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
			WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
		}

		GPUDoneWithFrame(m_fenceValues[m_frameIndex]);

		// Set the fence value for the next frame.
		m_fenceValues[m_frameIndex] = currentFenceValue + 1;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	class D3D12PipelineState : public PipelineState
	{
	protected:
		ComPtr<ID3D12PipelineState> _state;

	public:
		ID3D12PipelineState* GetState()
		{
			return _state.Get();
		}

		void Initialize(
			EBlendState InBlendState,
			ERasterizerState InRasterizerState,
			EDepthState InDepthState,
			std::shared_ptr < GPUInputLayout > InLayout,
			std::shared_ptr< GPUShader> InVS,
			std::shared_ptr< GPUShader> InPS,

			std::shared_ptr< GPUShader> InMS = nullptr,
			std::shared_ptr< GPUShader> InAS = nullptr,
			std::shared_ptr< GPUShader> InHS = nullptr,
			std::shared_ptr< GPUShader> InDS = nullptr,
			
			std::shared_ptr< GPUShader> InCS = nullptr)
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();

			SE_ASSERT(InVS || InMS || InCS);
			SE_ASSERT(InCS || InPS);
			SE_ASSERT(InLayout || InMS || InCS);

			auto& ourLayout = InLayout->GetAs<D3D12InputLayout>();			

			// vert shader pipeline
			if (InVS)
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				psoDesc.pRootSignature = InVS->GetAs<D3D12Shader>().GetRootSignature();
				psoDesc.InputLayout = { ourLayout.GetData(), (UINT)ourLayout.GetCount() };

				psoDesc.VS = CD3DX12_SHADER_BYTECODE(InVS->GetAs<D3D12Shader>().GetByteCode());
				if(InPS) psoDesc.PS = CD3DX12_SHADER_BYTECODE(InPS->GetAs<D3D12Shader>().GetByteCode());
				if (InHS) psoDesc.HS = CD3DX12_SHADER_BYTECODE(InHS->GetAs<D3D12Shader>().GetByteCode());				
				if (InDS) psoDesc.DS = CD3DX12_SHADER_BYTECODE(InDS->GetAs<D3D12Shader>().GetByteCode());
						
				psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				psoDesc.PrimitiveTopologyType = InHS ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
				psoDesc.SampleDesc = DefaultSampleDesc();
				psoDesc.SampleMask = UINT_MAX;

				if (InHS)
				{
					psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
					psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				}

				auto psoStream = CD3DX12_PIPELINE_STATE_STREAM2(psoDesc);

				D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
				streamDesc.pPipelineStateSubobjectStream = &psoStream;
				streamDesc.SizeInBytes = sizeof(psoStream);

				if (FAILED(pd3dDevice->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&_state))))
				{
					throw std::runtime_error("Failed to create graphics pipeline state");
				}

				DXSetName(_state.Get(), L"PSO Vertex Shader");
			}
			else if (InMS)
			{
				D3DX12_MESH_SHADER_PIPELINE_STATE_DESC psoDesc = {};

				psoDesc.pRootSignature = InMS->GetAs<D3D12Shader>().GetRootSignature();

				psoDesc.MS = CD3DX12_SHADER_BYTECODE(InMS->GetAs<D3D12Shader>().GetByteCode());
				if(InAS)psoDesc.AS = CD3DX12_SHADER_BYTECODE(InAS->GetAs<D3D12Shader>().GetByteCode());
				if(InPS)psoDesc.PS = CD3DX12_SHADER_BYTECODE(InPS->GetAs<D3D12Shader>().GetByteCode());

				psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
				psoDesc.SampleDesc = DefaultSampleDesc();
				psoDesc.SampleMask = UINT_MAX;

#if 0
				{
					psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
					psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				}
#endif

				auto psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(psoDesc);

				D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
				streamDesc.pPipelineStateSubobjectStream = &psoStream;
				streamDesc.SizeInBytes = sizeof(psoStream);

				if (FAILED(pd3dDevice->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&_state))))
				{
					throw std::runtime_error("Failed to create graphics pipeline state");
				}

				DXSetName(_state.Get(), L"PSO Mesh");
			}
			else if (InCS)
			{
				D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
				psoDesc.CS = CD3DX12_SHADER_BYTECODE(InCS->GetAs<D3D12Shader>().GetByteCode());
				psoDesc.pRootSignature = InCS->GetAs<D3D12Shader>().GetRootSignature();
												
				if (FAILED(pd3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&_state))))
				{
					throw std::runtime_error("Failed to create graphics pipeline state");
				}

				DXSetName(_state.Get(), L"PSO Compute");
			}
		}
	};

	class D3D12RenderableMesh : public RenderableMesh
	{
	protected:
		std::shared_ptr<D3D12PipelineState> _state;
		bool _bIsStatic = false;

	public:
		D3D12RenderableMesh(bool IsStatic) : _bIsStatic(IsStatic) {}
		virtual bool IsStatic() const {
			return _bIsStatic;
		}
		virtual void AddToScene(class RenderScene* InScene) override;
		virtual void Draw() override;
		virtual void DrawDebug(std::vector< DebugVertex >& lines) override;
	};

	std::shared_ptr<RenderableMesh> CreateRenderableMesh(bool bIsStatic)
	{
		return std::make_shared< D3D12RenderableMesh >(bIsStatic);
	}

	struct D3D12PipelineStateKey
	{
		EBlendState blendState = EBlendState::Disabled;
		ERasterizerState rasterizerState = ERasterizerState::BackFaceCull;
		EDepthState depthState = EDepthState::Enabled;

		uintptr_t inputLayout = 0;

		uintptr_t vs = 0;
		uintptr_t ps = 0;
		uintptr_t ms = 0;
		uintptr_t as = 0;
		uintptr_t hs = 0;
		uintptr_t ds = 0;
		uintptr_t cs = 0;

		bool operator<(const D3D12PipelineStateKey& compareKey)const
		{
			if (blendState != compareKey.blendState)
			{
				return blendState < compareKey.blendState;
			}
			if (rasterizerState != compareKey.rasterizerState)
			{
				return rasterizerState < compareKey.rasterizerState;
			}
			if (depthState != compareKey.depthState)
			{
				return depthState < compareKey.depthState;
			}
			if (inputLayout != compareKey.inputLayout)
			{
				return inputLayout < compareKey.inputLayout;
			}

			if (vs != compareKey.vs)
			{
				return vs < compareKey.vs;
			}
			if (ps != compareKey.ps)
			{
				return ps < compareKey.ps;
			}
			if (ms != compareKey.ms)
			{
				return ms < compareKey.ms;
			}
			if (as != compareKey.as)
			{
				return as < compareKey.as;
			}
			if (hs != compareKey.hs)
			{
				return hs < compareKey.hs;
			}
			if (ds != compareKey.ds)
			{
				return ds < compareKey.ds;
			}
			if (cs != compareKey.cs)
			{
				return cs < compareKey.cs;
			}

			return false;
		}
	};

	static std::map< D3D12PipelineStateKey, std::shared_ptr< D3D12PipelineState > > PiplineStateMap;

	std::shared_ptr < D3D12PipelineState >  GetD3D12PipelineState(EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		std::shared_ptr < GPUInputLayout > InLayout,
		std::shared_ptr< GPUShader> InVS,
		std::shared_ptr< GPUShader> InPS,
		std::shared_ptr< GPUShader> InMS,
		std::shared_ptr< GPUShader> InAS,
		std::shared_ptr< GPUShader> InHS,
		std::shared_ptr< GPUShader> InDS,
		std::shared_ptr< GPUShader> InCS)
	{
		D3D12PipelineStateKey key{ InBlendState, InRasterizerState, InDepthState,
			(uintptr_t)InLayout.get(),
			(uintptr_t)InVS.get(),
			(uintptr_t)InPS.get(),
			(uintptr_t)InMS.get(),
			(uintptr_t)InAS.get(),
			(uintptr_t)InHS.get(),
			(uintptr_t)InDS.get(),
			(uintptr_t)InCS.get() };

		auto findKey = PiplineStateMap.find(key);

		if (findKey == PiplineStateMap.end())
		{
			auto newPipelineState = std::make_shared< D3D12PipelineState >();
			newPipelineState->Initialize(InBlendState, InRasterizerState, InDepthState, InLayout, InVS, InPS, InMS, InAS, InHS, InDS, InCS);
			PiplineStateMap[key] = newPipelineState;
			return newPipelineState;
		}

		return findKey->second;
	}

	class D3D12ComputeDispatch : public GPUComputeDispatch
	{
	protected:
		std::shared_ptr<D3D12PipelineState> _state;

	public:

		D3D12ComputeDispatch(std::shared_ptr< GPUShader> InCS) : GPUComputeDispatch(InCS) { }
		
		virtual void Dispatch(const Vector3i &ThreadGroupCounts) override
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();
			auto perDrawSratchMem = GGraphicsDevice->GetPerDrawScratchMemory();
			auto perDrawDescriptorHeap = GGraphicsDevice->GetDynamicDescriptorHeap();
			auto perDrawSamplerHeap = GGraphicsDevice->GetDynamicSamplerHeap();
			auto cmdList = GGraphicsDevice->GetCommandList();
			auto currentFrame = GGraphicsDevice->GetFrameCount();

			if (!_state)
			{
				_state = GetD3D12PipelineState(EBlendState::Disabled,
					ERasterizerState::NoCull,
					EDepthState::Disabled,
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					_compute);
			}	

			ID3D12RootSignature* rootSig = _compute->GetAs<D3D12Shader>().GetRootSignature();
			cmdList->SetComputeRootSignature(rootSig);
			cmdList->SetPipelineState(_state->GetState());


			for (int32_t Iter = 0; Iter < _constants.size(); Iter++)
			{
				auto currentResource = _constants[Iter];
				auto HeapAddrs = perDrawSratchMem->GetWritable(currentResource->GetTotalSize(), currentFrame);;
				WriteMem(HeapAddrs, currentResource->GetElementData(), currentResource->GetTotalSize());

				cmdList->SetComputeRootConstantBufferView(Iter, HeapAddrs.gpuAddr);
			}

			auto UAVSlotBlock = perDrawDescriptorHeap.GetDescriptorSlots(_textures.size());

			for (int32_t Iter = 0; Iter < _textures.size(); Iter++)
			{
				auto UAVDescriptor = UAVSlotBlock[Iter];

				auto currentResource = _textures[Iter];
				auto& currentTexture = currentResource->GetAs<D3D12Texture>();				
				auto& description = currentTexture.GetDescription();

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = description.Format;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			
				pd3dDevice->CreateUnorderedAccessView(currentTexture.GetTexture(), nullptr, &uavDesc, UAVDescriptor.cpuHandle);
			}

			cmdList->SetComputeRootDescriptorTable(1, UAVSlotBlock.gpuHandle);

			cmdList->Dispatch(ThreadGroupCounts[0], ThreadGroupCounts[1], ThreadGroupCounts[2]);
		}
	};

	std::shared_ptr<GPUComputeDispatch> DX_12CreateComputeDispatch(std::shared_ptr< GPUShader> InCS)
	{
		return std::make_shared< D3D12ComputeDispatch >(InCS);
	}

	static Vector3d HACKS_CameraPos;

	void DrawAABB(const AABB& InAABB, std::vector< DebugVertex > &lines)
	{
		auto minValue = InAABB.GetMin().cast<float>();
		auto maxValue = InAABB.GetMax().cast<float>();

		Vector3 topPoints[4];
		Vector3 bottomPoints[4];

		topPoints[0] = Vector3(minValue[0], minValue[1], minValue[2]);
		topPoints[1] = Vector3(maxValue[0], minValue[1], minValue[2]);
		topPoints[2] = Vector3(maxValue[0], minValue[1], maxValue[2]);
		topPoints[3] = Vector3(minValue[0], minValue[1], maxValue[2]);

		bottomPoints[0] = Vector3(minValue[0], maxValue[1], minValue[2]);
		bottomPoints[1] = Vector3(maxValue[0], maxValue[1], minValue[2]);
		bottomPoints[2] = Vector3(maxValue[0], maxValue[1], maxValue[2]);
		bottomPoints[3] = Vector3(minValue[0], maxValue[1], maxValue[2]);

		for (int32_t Iter = 0; Iter < 4; Iter++)
		{
			int32_t nextPoint = (Iter + 1) % 4;

			lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
			lines.push_back({ topPoints[nextPoint], Vector3(1,1,1) });

			lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
			lines.push_back({ bottomPoints[nextPoint], Vector3(1,1,1) });

			lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
			lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
		}
	}

	class D3D12RenderScene : public RenderScene
	{
	protected:
		D3D12PartialResourceMemory _currentFrameMem;
		//std::vector< D3D12RenderableMesh* > _renderMeshes;
		bool _bMeshInstancesDirty = false;

		std::shared_ptr< GPUShader > _debugVS;
		std::shared_ptr< GPUShader > _debugPS;
		std::shared_ptr< D3D12PipelineState > _debugPSO;
		std::shared_ptr< GPUInputLayout > _debugLayout;

		std::shared_ptr < ArrayResource >  _debugResource;
		std::shared_ptr< GPUBuffer > _debugBuffer;

		std::vector< DebugVertex > _lines;

	public:
		D3D12RenderScene()
		{
			_debugVS = DX12_CreateShader(EShaderType::Vertex);
			_debugVS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_vs");

			_debugPS = DX12_CreateShader(EShaderType::Pixel);
			_debugPS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_ps");

			_debugLayout = DX12_CreateInputLayout();
			_debugLayout->InitializeLayout({
					{ "POSITION",  InputLayoutElementType::Float3, offsetof(DebugVertex,position) },
					{ "COLOR",  InputLayoutElementType::Float3, offsetof(DebugVertex,color) }
				});

			_debugPSO = GetD3D12PipelineState(EBlendState::Disabled, 
				ERasterizerState::NoCull, 
				EDepthState::Disabled, 
				_debugLayout, 
				_debugVS, 
				_debugPS, 
				nullptr,
				nullptr, 
				nullptr, 
				nullptr,
				nullptr);

			_debugResource = std::make_shared< ArrayResource >();
			_debugResource->InitializeFromType< DebugVertex >(10 * 1024);
			_debugBuffer = DX12_CreateStaticBuffer(GPUBufferType::Vertex, _debugResource);
		}

		void DrawDebug()
		{
			auto pd3dDevice = GGraphicsDevice->GetDevice();
			auto perDrawSratchMem = GGraphicsDevice->GetPerDrawScratchMemory();
			auto cmdList = GGraphicsDevice->GetCommandList();
			auto currentFrame = GGraphicsDevice->GetFrameCount();

			//_octree.WalkNodes([&](const AABBi& InAABB) -> bool
			//	{
			//		auto minValue = InAABB.GetMin().cast<float>();
			//		auto maxValue = InAABB.GetMax().cast<float>();

			//		Vector3 topPoints[4];
			//		Vector3 bottomPoints[4];

			//		topPoints[0] = Vector3(minValue[0], minValue[1], minValue[2]);
			//		topPoints[1] = Vector3(maxValue[0], minValue[1], minValue[2]);
			//		topPoints[2] = Vector3(maxValue[0], minValue[1], maxValue[2]);
			//		topPoints[3] = Vector3(minValue[0], minValue[1], maxValue[2]);

			//		bottomPoints[0] = Vector3(minValue[0], maxValue[1], minValue[2]);
			//		bottomPoints[1] = Vector3(maxValue[0], maxValue[1], minValue[2]);
			//		bottomPoints[2] = Vector3(maxValue[0], maxValue[1], maxValue[2]);
			//		bottomPoints[3] = Vector3(minValue[0], maxValue[1], maxValue[2]);

			//		for (int32_t Iter = 0; Iter < 4; Iter++)
			//		{
			//			int32_t nextPoint = (Iter + 1) % 4;

			//			_lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
			//			_lines.push_back({ topPoints[nextPoint], Vector3(1,1,1) });

			//			_lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
			//			_lines.push_back({ bottomPoints[nextPoint], Vector3(1,1,1) });

			//			_lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
			//			_lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
			//		}

			//		return true;
			//	});

			ID3D12RootSignature* rootSig = nullptr;

			if (_debugVS)
			{
				rootSig = _debugVS->GetAs<D3D12Shader>().GetRootSignature();
			}

			cmdList->SetGraphicsRootSignature(rootSig);

			//table 0, shared all constant, scene stuff 
			{
				cmdList->SetGraphicsRootConstantBufferView(0, GetGPUAddrOfViewConstants());

				CD3DX12_VIEWPORT m_viewport(0.0f, 0.0f, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
				CD3DX12_RECT m_scissorRect(0, 0, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
				cmdList->RSSetViewports(1, &m_viewport);
				cmdList->RSSetScissorRects(1, &m_scissorRect);
			}

			//table 1, VS only constants
			{
				auto pd3dDevice = GGraphicsDevice->GetDevice();

				_declspec(align(256u))
				struct GPUDrawConstants
				{
					//altered viewposition translated
					Matrix4x4 LocalToWorldScaleRotation;
					Vector3d Translation;
				};

				Matrix4x4 matId = Matrix4x4::Identity();
				Vector3d dummyVec(0, 0, 0);

				// write local to world
				auto HeapAddrs = perDrawSratchMem->GetWritable(sizeof(GPUDrawConstants), currentFrame);
				WriteMem(HeapAddrs, offsetof(GPUDrawConstants, LocalToWorldScaleRotation), matId);
				WriteMem(HeapAddrs, offsetof(GPUDrawConstants, Translation), dummyVec);

				cmdList->SetGraphicsRootConstantBufferView(1, HeapAddrs.gpuAddr);
			}

			cmdList->SetPipelineState(_debugPSO->GetState());
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

			auto perFrameSratchMem = GGraphicsDevice->GetPerDrawScratchMemory();

			auto heapChunk = perFrameSratchMem->GetWritable(_lines.size() * sizeof(DebugVertex), currentFrame);

			memcpy(heapChunk.cpuAddr, _lines.data(), _lines.size() * sizeof(DebugVertex));

			D3D12_VERTEX_BUFFER_VIEW vbv;
			vbv.BufferLocation = heapChunk.gpuAddr;
			vbv.StrideInBytes = sizeof(DebugVertex);
			vbv.SizeInBytes = heapChunk.size;;
			cmdList->IASetVertexBuffers(0, 1, &vbv);
			cmdList->DrawInstanced(_lines.size(), 1, 0, 0);

			_lines.clear();
		}

		virtual void Build() {};

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddrOfViewConstants()
		{
			return _currentFrameMem.gpuAddr;
		}

		virtual void AddToScene(Renderable* InRenderable) override
		{
			RenderScene::AddToScene(InRenderable);
		
			//HACKS FIX

		/*	auto thisMesh = (D3D12RenderableMesh*)InRenderable;
			if (thisMesh->IsStatic())
			{
				_renderMeshes.push_back(thisMesh);
				_bMeshInstancesDirty = true;
			}	*/		
		}

		virtual void RemoveFromScene(Renderable* InRenderable)
		{
			RenderScene::RemoveFromScene(InRenderable);

			//HACKS FIX

			//auto thisMesh = (D3D12RenderableMesh*)InRenderable;
			//if (thisMesh->IsStatic())
			//{
			//	//_renderMeshes.erase(thisMesh);
			//	_bMeshInstancesDirty = true;
			//}
		}

		virtual void Draw()
		{
			// if mesh instances dirty update that structure
			//if (_bMeshInstancesDirty)
			//{

			//}

			//TODO SHARED STRUCTURE FOR CROSS HLSL STRUCTS!!
			// 
			//on GPU
			//struct _ViewConstants
			//{
			//	//all origin centered
			//	float4x4 ViewMatrix;
			//	float4x4 ViewProjectionMatrix;
			//	float4x4 InvViewProjectionMatrix;
			//	//real view position
			//	double3 ViewPosition;
			//};

			_declspec(align(256u))
			struct GPUViewConstants
			{
				//all origin centered
				Matrix4x4 ViewMatrix;
				Matrix4x4 ViewProjectionMatrix;
				Matrix4x4 InvViewProjectionMatrix;
				//real view position
				Vector3d ViewPosition;
				Vector4d FrustumPlanes[6];
				float RecipTanHalfFovy;
			};

			_view.GenerateLeftHandFoVPerspectiveMatrix(45.0f, (float)GGraphicsDevice->GetDeviceWidth() / (float)GGraphicsDevice->GetDeviceHeight());
			_view.BuildCameraMatrices();

			auto perFrameSratchMem = GGraphicsDevice->GetPerDrawScratchMemory();
			auto currentFrame = GGraphicsDevice->GetFrameCount();

			Planed frustumPlanes[6];
			_view.GetFrustumPlanes(frustumPlanes);

			// get first index
			_currentFrameMem = perFrameSratchMem->GetWritable(sizeof(GPUViewConstants), currentFrame);
				
			WriteMem(_currentFrameMem, offsetof(GPUViewConstants, ViewMatrix), _view.GetCameraMatrix());
			WriteMem(_currentFrameMem, offsetof(GPUViewConstants, ViewProjectionMatrix), _view.GetViewProjMatrix());
			WriteMem(_currentFrameMem, offsetof(GPUViewConstants, InvViewProjectionMatrix), _view.GetInvViewProjMatrix());
			WriteMem(_currentFrameMem, offsetof(GPUViewConstants, ViewPosition), _view.GetCameraPosition());

			// FIXME
			HACKS_CameraPos = _view.GetCameraPosition();

			for (int32_t Iter = 0; Iter < ARRAY_SIZE(frustumPlanes); Iter++)
			{
				WriteMem(_currentFrameMem, offsetof(GPUViewConstants, FrustumPlanes[Iter]), frustumPlanes[Iter].coeffs());
			}

			WriteMem(_currentFrameMem, offsetof(GPUViewConstants, RecipTanHalfFovy), _view.GetRecipTanHalfFovy());
			
			/*
			_octree.WalkElements(frustumPlanes, [](const IOctreeElement* InElement) -> bool
				{
					((Renderable*)InElement)->Draw();
					return true;
				});
			*/

			//for (auto renderItem : _renderables)
			//{
			//	renderItem->DrawDebug(_lines);
			//}

			//DrawDebug();

			for (auto renderItem : _renderables)
			{
				renderItem->Draw();
			}
		};
	};
		
	void D3D12RenderableMesh::AddToScene(class RenderScene* InScene)
	{
		auto _meshData = _meshElements.front();
		
		RenderableMesh::AddToScene(InScene);
		_state = GetD3D12PipelineState(
			_meshData->material->blendState,
			_meshData->material->rasterizerState,
			_meshData->material->depthState,
			_meshData->material->layout,
			_meshData->material->vertexShader,
			_meshData->material->pixelShader,
			_meshData->material->meshShader,
			_meshData->material->amplificationShader,
			_meshData->material->hullShader,
			_meshData->material->domainShader,
			nullptr);

		_cachedRotationScale = Matrix4x4::Identity();
		_cachedRotationScale.block<3, 3>(0, 0) = GenerateRotationScale();
	}

	template<typename F>
	void NodeTraversal(const Matrix4x4 &InTransform,
		uint32_t CurrentIdx, 
		const std::vector<MeshNode> &MeshletNodes, 
		const Vector3 &InCamPos, 
		uint32_t CurrentLevel,
		const F &func)
	{
		auto& curNode = MeshletNodes[CurrentIdx];

		AABB transformedAABB = curNode.Bounds.Transform(InTransform);
		float Radius = transformedAABB.Extent().norm();
		float DistanceToCamera = std::max( (HACKS_CameraPos.cast<float>() - transformedAABB.Center()).norm() - Radius, 0.0f);


		float DistanceFactor = (DistanceToCamera / 100.0f) * (CurrentLevel+1);

		auto StartIdx = std::get<0>(curNode.ChildrenRange);
		auto EndIdx = std::get<1>(curNode.ChildrenRange);

		auto ChildCount = EndIdx - StartIdx;

		if (DistanceFactor < 10.0f && ChildCount > 0)
		{
			uint32_t StartIdx = std::get<0>(curNode.ChildrenRange);
			uint32_t EndIdx = std::get<1>(curNode.ChildrenRange);

			for (uint32_t IdxIter = StartIdx; IdxIter < EndIdx; IdxIter++)
			{
				NodeTraversal(InTransform, IdxIter, MeshletNodes, InCamPos, CurrentLevel+1, func);
			}
		}
		else
		{			
			func(CurrentIdx);
		}
	}

	void D3D12RenderableMesh::DrawDebug(std::vector< DebugVertex >& lines)
	{
		auto localToWorld = GenerateLocalToWorldMatrix();
		for (auto _meshData : _meshElements)
		{
			if (_meshData->MeshletNodes.size())
			{
				NodeTraversal(localToWorld, 0, _meshData->MeshletNodes, HACKS_CameraPos.cast<float>(), 0, [&](uint32_t IdxIter)
					{
						auto& renderNode = _meshData->MeshletNodes[IdxIter];
						auto meshletCount = std::get<1>(renderNode.MeshletRange) - std::get<0>(renderNode.MeshletRange);

						auto transformedAABB = renderNode.Bounds.Transform(localToWorld);
						DrawAABB(transformedAABB, lines);
					});
			}
		}
	}

	void D3D12RenderableMesh::Draw()
	{		
		auto pd3dDevice = GGraphicsDevice->GetDevice();
		auto perDrawSratchMem = GGraphicsDevice->GetPerDrawScratchMemory();
		auto perDrawDescriptorHeap = GGraphicsDevice->GetDynamicDescriptorHeap();
		auto perDrawSamplerHeap = GGraphicsDevice->GetDynamicSamplerHeap();
		auto cmdList = GGraphicsDevice->GetCommandList();
		auto currentFrame = GGraphicsDevice->GetFrameCount();

		for (auto _meshData : _meshElements)
		{
			ID3D12RootSignature* rootSig = nullptr;

			if (_meshData->material->vertexShader)
			{
				rootSig = _meshData->material->vertexShader->GetAs<D3D12Shader>().GetRootSignature();
			}
			else
			{
				rootSig = _meshData->material->meshShader->GetAs<D3D12Shader>().GetRootSignature();
			}

			cmdList->SetGraphicsRootSignature(rootSig);

			//table 0, shared all constant, scene stuff 
			{
				cmdList->SetGraphicsRootConstantBufferView(0, _parentScene->GetAs<D3D12RenderScene>().GetGPUAddrOfViewConstants());

				CD3DX12_VIEWPORT m_viewport(0.0f, 0.0f, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
				CD3DX12_RECT m_scissorRect(0, 0, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
				cmdList->RSSetViewports(1, &m_viewport);
				cmdList->RSSetScissorRects(1, &m_scissorRect);
			}


			//table 1, VS only constants
			{
				auto pd3dDevice = GGraphicsDevice->GetDevice();

				_declspec(align(256u))
				struct GPUDrawConstants
				{
					//altered viewposition translated
					Matrix4x4 LocalToWorldScaleRotation;
					Vector3d Translation;
				};

				// write local to world
				auto HeapAddrs = perDrawSratchMem->GetWritable(sizeof(GPUDrawConstants), currentFrame);
				WriteMem(HeapAddrs, offsetof(GPUDrawConstants, LocalToWorldScaleRotation), _cachedRotationScale);
				WriteMem(HeapAddrs, offsetof(GPUDrawConstants, Translation), _position);

				cmdList->SetGraphicsRootConstantBufferView(1, HeapAddrs.gpuAddr);
			}

			//2&3 pixel shaders
			//4 domain
			//5,6 mesh

			//table 4&5 SRV and SAMPLERS
			{
				if (_meshData->material->textureArray.size())
				{
					auto SRVSlotBlock = perDrawDescriptorHeap.GetDescriptorSlots((uint8_t)_meshData->material->textureArray.size());
					auto SamplerSlotBlock = perDrawSamplerHeap.GetDescriptorSlots((uint8_t)_meshData->material->textureArray.size());

					// Describe and create a SRV for the texture.
					for (int32_t Iter = 0; Iter < _meshData->material->textureArray.size(); Iter++)
					{
						SE_ASSERT(_meshData->material->textureArray[Iter]);

						{
							auto psSRVDescriptor = SRVSlotBlock[Iter];

							auto texRef = _meshData->material->textureArray[Iter]->GetAs<D3D12Texture>();

							auto& description = texRef.GetDescription();

							D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
							srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
							srvDesc.Format = description.Format;
							srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
							srvDesc.Texture2D.MipLevels = description.MipLevels;
							pd3dDevice->CreateShaderResourceView(texRef.GetTexture(), &srvDesc, psSRVDescriptor.cpuHandle);
						}

						{
							auto psSamplerDescriptor = SamplerSlotBlock[Iter];

							D3D12_SAMPLER_DESC wrapSamplerDesc = {};
							wrapSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
							wrapSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
							wrapSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
							wrapSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
							wrapSamplerDesc.MinLOD = 0;
							wrapSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
							wrapSamplerDesc.MipLODBias = 0.0f;
							wrapSamplerDesc.MaxAnisotropy = 1;
							wrapSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
							wrapSamplerDesc.BorderColor[0] = wrapSamplerDesc.BorderColor[1] = wrapSamplerDesc.BorderColor[2] = wrapSamplerDesc.BorderColor[3] = 0;

							pd3dDevice->CreateSampler(&wrapSamplerDesc, psSamplerDescriptor.cpuHandle);
						}
					}

					//need to set to CBV, or first one
					cmdList->SetGraphicsRootDescriptorTable(7, SRVSlotBlock.gpuHandle);
					cmdList->SetGraphicsRootDescriptorTable(12, SamplerSlotBlock.gpuHandle);
				}
			}

			cmdList->SetPipelineState(_state->GetState());

			switch (_meshData->topology)
			{
			case EDrawingTopology::PointList:
				cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
				break;
			case EDrawingTopology::LineList:
				cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
				break;
			case EDrawingTopology::TriangleList:
				cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				break;
			case EDrawingTopology::PatchList_4ControlPoints:
				cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
				break;
			default:
				// must have useful topology
				SE_ASSERT(false);
				break;
			}


			//cmdList->SetComputeRootUnorderedAccessView

			if (_meshData->material->vertexShader)
			{
				cmdList->IASetVertexBuffers(0, 1, _meshData->VertexResource->GetAs<D3D12VertexBuffer>().GetView());

				if (_meshData->IndexResource)
				{
					cmdList->IASetIndexBuffer(_meshData->IndexResource->GetAs<D3D12IndexBuffer>().GetView());
					cmdList->DrawIndexedInstanced(_meshData->IndexResource->GetAs<D3D12IndexBuffer>().GetCachedElementCount(), 1, 0, 0, 0);
				}
				else
				{
					//cmdList->DrawInstanced(static_cast<UINT>(mVertexCount - mBaseVertex), 1, 0, 0);
				}
			}
			else if (_meshData->material->meshShader)
			{
				if (_meshData->MeshIndex >= 0)
				{					
					ReadyMeshElement(_meshData);

					cmdList->SetGraphicsRoot32BitConstant(6, _meshData->MeshIndex, 0);
					cmdList->SetGraphicsRoot32BitConstant(6, 4, 1);

#if 1
					auto& StartingNode = _meshData->MeshletNodes.front();

					float DistanceToCamera = (HACKS_CameraPos.cast<float>() - StartingNode.Bounds.Center()).norm();

					float DistanceFactor = StartingNode.TriCount / DistanceToCamera;

					auto localToWorld = GenerateLocalToWorldMatrix();

					NodeTraversal(localToWorld, 0, _meshData->MeshletNodes, HACKS_CameraPos.cast<float>(), 0, [&](uint32_t IdxIter)
						{
							auto& renderNode = _meshData->MeshletNodes[IdxIter];
							auto meshletCount = std::get<1>(renderNode.MeshletRange) - std::get<0>(renderNode.MeshletRange);
							cmdList->SetGraphicsRoot32BitConstant(6, std::get<0>(renderNode.MeshletRange), 2);
							cmdList->SetGraphicsRoot32BitConstant(6, meshletCount, 3);
							cmdList->SetGraphicsRoot32BitConstant(6, IdxIter, 4);
							cmdList->DispatchMesh(DivRoundUp(meshletCount, 32), 1, 1);
						});

					//if (DistanceFactor > 16)
					//{
					//	uint32_t StartIdx = std::get<0>(_meshData->MeshletNodes[0].ChildrenRange);
					//	uint32_t EndIdx = std::get<1>(_meshData->MeshletNodes[0].ChildrenRange);
					//	for (uint32_t IdxIter = StartIdx; IdxIter < EndIdx; IdxIter++)
					//	{
					//		auto& renderNode = _meshData->MeshletNodes[IdxIter];
					//		auto meshletCount = std::get<1>(renderNode.MeshletRange) - std::get<0>(renderNode.MeshletRange);
					//		cmdList->SetGraphicsRoot32BitConstant(6, std::get<0>(renderNode.MeshletRange), 2);
					//		cmdList->SetGraphicsRoot32BitConstant(6, meshletCount, 3);
					//		cmdList->DispatchMesh(DivRoundUp(meshletCount, 32), 1, 1);
					//	}						
					//}
					//else
					//{
					//	auto& renderNode = _meshData->MeshletNodes[0];
					//	auto meshletCount = std::get<1>(renderNode.MeshletRange) - std::get<0>(renderNode.MeshletRange);
					//	cmdList->SetGraphicsRoot32BitConstant(6, std::get<0>(renderNode.MeshletRange), 2);
					//	cmdList->SetGraphicsRoot32BitConstant(6, meshletCount, 3);
					//	cmdList->DispatchMesh(DivRoundUp(meshletCount, 32), 1, 1);
					//}
#else
					cmdList->SetGraphicsRoot32BitConstant(6, _meshData->MeshletSubsets.front().Count, 3);

					for (auto& subset : _meshData->MeshletSubsets)
					{
						cmdList->DispatchMesh(DivRoundUp(subset.Count, 32), 1, 1);
					}
#endif
				}
			}
		}
	}

	void BegineResourceCopy()
	{
		SE_ASSERT(GGraphicsDevice);
		GGraphicsDevice->BeginResourceCopy();
	}

	void EndResourceCopy()
	{
		SE_ASSERT(GGraphicsDevice);
		GGraphicsDevice->EndResourceCopy();
	}
	

	std::shared_ptr< GraphicsDevice > DX12_CreateGraphicsDevice()
	{
		return std::make_shared< DX12Device>();
	}
		
	struct DXGraphicInterface : public IGraphicsInterface
	{
		// hacky so one GGI per DLL
		DXGraphicInterface()
		{
			SET_GGI(this);
		}

		virtual std::shared_ptr< GPUShader > CreateShader(EShaderType InType) override
		{			
			return DX12_CreateShader(InType);
		}
		virtual std::shared_ptr< GPUComputeDispatch > CreateComputeDispatch(std::shared_ptr< GPUShader> InCS) override
		{
			return DX_12CreateComputeDispatch(InCS);
		}
		virtual std::shared_ptr< GPUBuffer > CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData = nullptr) override
		{
			return DX12_CreateStaticBuffer(InType, InCpuData);
		}
		virtual std::shared_ptr< GPUInputLayout > CreateInputLayout() override
		{
			return DX12_CreateInputLayout();
		}
		virtual std::shared_ptr< GPUTexture > CreateTexture(int32_t Width, int32_t Height, TextureFormat Format,
			std::shared_ptr< ArrayResource > RawData = nullptr,
			std::shared_ptr< ImageMeta > InMetaInfo = nullptr) override
		{
			return DX12_CreateTexture(Width, Height, Format, RawData, InMetaInfo);
		}
		virtual std::shared_ptr< GPURenderTarget > CreateRenderTarget() override
		{
			return DX12_CreateRenderTarget();
		}
		virtual std::shared_ptr< GraphicsDevice > CreateGraphicsDevice() override
		{
			return DX12_CreateGraphicsDevice();
		}

		virtual std::shared_ptr<RenderScene> CreateRenderScene() override
		{
			return std::make_shared< D3D12RenderScene >();
		}
	};

	static DXGraphicInterface staticDGI;
}