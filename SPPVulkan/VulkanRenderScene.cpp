// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"
#include "VulkanDevice.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"

namespace SPP
{
	extern LogEntry LOG_VULKAN;

	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	// lazy externs
	extern GPUReferencer< GPUShader > Vulkan_CreateShader(EShaderType InType);
	extern GPUReferencer< GPUBuffer > Vulkan_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	extern GPUReferencer< GPUInputLayout > Vulkan_CreateInputLayout();

	static Vector3d HACKS_CameraPos;

	VulkanRenderScene::VulkanRenderScene()
	{
		_debugVS = Vulkan_CreateShader(EShaderType::Vertex);
		_debugVS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_vs");

		_debugPS = Vulkan_CreateShader(EShaderType::Pixel);
		_debugPS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_ps");


		_debugLayout = Vulkan_CreateInputLayout();

		{
			auto& vulkanInputLayout = _debugLayout->GetAs<VulkanInputLayout>();
			DebugVertex dvPattern;
			vulkanInputLayout.Begin();
			vulkanInputLayout.AddVertexStream(dvPattern, dvPattern.position, dvPattern.color);
			vulkanInputLayout.Finalize();
		}
			
		//_debugLayout->InitializeLayout({
		//		{ "POSITION",  InputLayoutElementType::Float3, offsetof(DebugVertex,position) },
		//		{ "COLOR",  InputLayoutElementType::Float3, offsetof(DebugVertex,color) }
		//	});

		_debugPSO = GetVulkanPipelineState(EBlendState::Disabled,
			ERasterizerState::NoCull,
			EDepthState::Enabled,
			EDrawingTopology::LineList,
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
		_debugBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Vertex, _debugResource);

		//
		_fullscreenRayVS = Vulkan_CreateShader(EShaderType::Vertex);
		_fullscreenRayVS->CompileShaderFromFile("shaders/fullScreenRayVS.hlsl", "main_vs");

		_fullscreenRaySDFPS = Vulkan_CreateShader(EShaderType::Pixel);
		_fullscreenRaySDFPS->CompileShaderFromFile("shaders/fullScreenRaySDFPS.hlsl", "main_ps");

		_fullscreenRaySkyBoxPS = Vulkan_CreateShader(EShaderType::Pixel);
		_fullscreenRaySkyBoxPS->CompileShaderFromFile("shaders/fullScreenRayCubemapPS.hlsl", "main_ps");

		_fullscreenRayVSLayout = Vulkan_CreateInputLayout();

		{
			auto& vulkanInputLayout = _fullscreenRayVSLayout->GetAs<VulkanInputLayout>();
			FullscreenVertex dvPattern;
			vulkanInputLayout.AddVertexStream(dvPattern, dvPattern.position);
			vulkanInputLayout.Finalize();
		}

		_fullscreenRaySDFPSO = GetVulkanPipelineState(EBlendState::Disabled,
			ERasterizerState::NoCull,
			EDepthState::Enabled,
			EDrawingTopology::TriangleList,
			_fullscreenRayVSLayout,
			_fullscreenRayVS,
			_fullscreenRaySDFPS,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr);

		_fullscreenSkyBoxPSO = GetVulkanPipelineState(EBlendState::Disabled,
			ERasterizerState::NoCull,
			EDepthState::Disabled,
			EDrawingTopology::TriangleList,
			_fullscreenRayVSLayout,
			_fullscreenRayVS,
			_fullscreenRaySkyBoxPS,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr);
	}

	//void VulkanRenderScene::DrawDebug()
	//{
		//if (_lines.empty()) return;

		//auto pd3dDevice = GGraphicsDevice->GetDevice();
		//auto perDrawSratchMem = GGraphicsDevice->GetPerFrameScratchMemory();
		//auto cmdList = GGraphicsDevice->GetCommandList();
		//auto currentFrame = GGraphicsDevice->GetFrameCount();

		////_octree.WalkNodes([&](const AABBi& InAABB) -> bool
		////	{
		////		auto minValue = InAABB.GetMin().cast<float>();
		////		auto maxValue = InAABB.GetMax().cast<float>();

		////		Vector3 topPoints[4];
		////		Vector3 bottomPoints[4];

		////		topPoints[0] = Vector3(minValue[0], minValue[1], minValue[2]);
		////		topPoints[1] = Vector3(maxValue[0], minValue[1], minValue[2]);
		////		topPoints[2] = Vector3(maxValue[0], minValue[1], maxValue[2]);
		////		topPoints[3] = Vector3(minValue[0], minValue[1], maxValue[2]);

		////		bottomPoints[0] = Vector3(minValue[0], maxValue[1], minValue[2]);
		////		bottomPoints[1] = Vector3(maxValue[0], maxValue[1], minValue[2]);
		////		bottomPoints[2] = Vector3(maxValue[0], maxValue[1], maxValue[2]);
		////		bottomPoints[3] = Vector3(minValue[0], maxValue[1], maxValue[2]);

		////		for (int32_t Iter = 0; Iter < 4; Iter++)
		////		{
		////			int32_t nextPoint = (Iter + 1) % 4;

		////			_lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
		////			_lines.push_back({ topPoints[nextPoint], Vector3(1,1,1) });

		////			_lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
		////			_lines.push_back({ bottomPoints[nextPoint], Vector3(1,1,1) });

		////			_lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
		////			_lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
		////		}

		////		return true;
		////	});

		//ID3D12RootSignature* rootSig = nullptr;

		//if (_debugVS)
		//{
		//	rootSig = _debugVS->GetAs<D3D12Shader>().GetRootSignature();
		//}

		//cmdList->SetGraphicsRootSignature(rootSig);

		////table 0, shared all constant, scene stuff 
		//{
		//	cmdList->SetGraphicsRootConstantBufferView(0, GetGPUAddrOfViewConstants());

		//	CD3DX12_VIEWPORT m_viewport(0.0f, 0.0f, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
		//	CD3DX12_RECT m_scissorRect(0, 0, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
		//	cmdList->RSSetViewports(1, &m_viewport);
		//	cmdList->RSSetScissorRects(1, &m_scissorRect);
		//}

		////table 1, VS only constants
		//{
		//	auto pd3dDevice = GGraphicsDevice->GetDevice();

		//	_declspec(align(256u))
		//		struct GPUDrawConstants
		//	{
		//		//altered viewposition translated
		//		Matrix4x4 LocalToWorldScaleRotation;
		//		Vector3d Translation;
		//	};

		//	Matrix4x4 matId = Matrix4x4::Identity();
		//	Vector3d dummyVec(0, 0, 0);

		//	// write local to world
		//	auto HeapAddrs = perDrawSratchMem->GetWritable(sizeof(GPUDrawConstants), currentFrame);
		//	WriteMem(HeapAddrs, offsetof(GPUDrawConstants, LocalToWorldScaleRotation), matId);
		//	WriteMem(HeapAddrs, offsetof(GPUDrawConstants, Translation), dummyVec);

		//	cmdList->SetGraphicsRootConstantBufferView(1, HeapAddrs.gpuAddr);
		//}

		//cmdList->SetPipelineState(_debugPSO->GetState());
		//cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

		//auto perFrameSratchMem = GGraphicsDevice->GetPerFrameScratchMemory();

		//auto heapChunk = perFrameSratchMem->GetWritable(_lines.size() * sizeof(DebugVertex), currentFrame);

		//memcpy(heapChunk.cpuAddr, _lines.data(), _lines.size() * sizeof(DebugVertex));

		//D3D12_VERTEX_BUFFER_VIEW vbv;
		//vbv.BufferLocation = heapChunk.gpuAddr;
		//vbv.StrideInBytes = sizeof(DebugVertex);
		//vbv.SizeInBytes = heapChunk.size;;
		//cmdList->IASetVertexBuffers(0, 1, &vbv);
		//cmdList->DrawInstanced(_lines.size(), 1, 0, 0);

		//_lines.clear();
	//}


	void VulkanRenderScene::AddToScene(Renderable* InRenderable)
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

	void VulkanRenderScene::RemoveFromScene(Renderable* InRenderable)
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

#define MAX_MESH_ELEMENTS 1024
#define MAX_TEXTURE_COUNT 2048
#define DYNAMIC_MAX_COUNT 20 * 1024

	//
	//static DescriptorTableConfig _tableRegions[] =
	//{
	//	{ HDT_ShapeInfos, 1 },
	//	{ HDT_MeshInfos, 1 },
	//	{ HDT_MeshletVertices, MAX_MESH_ELEMENTS },
	//	{ HDT_MeshletResource, MAX_MESH_ELEMENTS },
	//	{ HDT_UniqueVertexIndices, MAX_MESH_ELEMENTS },
	//	{ HDT_PrimitiveIndices, MAX_MESH_ELEMENTS },

	//	{ HDT_Textures, MAX_TEXTURE_COUNT },
	//	{ HDT_Dynamic, DYNAMIC_MAX_COUNT },
	//};
	//

	void VulkanRenderScene::Draw()
	{
		extern VkDevice GGlobalVulkanDevice;
		extern VulkanGraphicsDevice* GGlobalVulkanGI;


		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();

		//auto pd3dDevice = GGraphicsDevice->GetDevice();
		//auto perDrawSratchMem = GGraphicsDevice->GetPerFrameScratchMemory();
		//auto cmdList = GGraphicsDevice->GetCommandList();
		//auto currentFrame = GGraphicsDevice->GetFrameCount();

		//auto backBufferColor = GGraphicsDevice->GetScreenColor();
		//auto backBufferDepth = GGraphicsDevice->GetScreenDepth();
		
		if (_bRenderToBackBuffer)
		{
			VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

			VkClearValue clearValues[2];
			//clearValues[0].color = defaultClearColor;
			clearValues[0].color = { { 0.25f, 0.25f, 0.25f, 1.0f } };;
			clearValues[1].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
			renderPassBeginInfo.renderPass = basicRenderPass;
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent.width = DeviceExtents[0];
			renderPassBeginInfo.renderArea.extent.height = DeviceExtents[1];
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;

			const VkViewport viewport = vks::initializers::viewport((float)DeviceExtents[0], (float)DeviceExtents[1], 0.0f, 1.0f);
			const VkRect2D scissor = vks::initializers::rect2D(DeviceExtents[0], DeviceExtents[1], 0, 0);
		}
		else
		{
			//
		}

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

		_view.GenerateLeftHandFoVPerspectiveMatrix(45.0f, (float)DeviceExtents[0] / (float)DeviceExtents[1]);
		_view.BuildCameraMatrices();

		Planed frustumPlanes[6];
		_view.GetFrustumPlanes(frustumPlanes);

		// get first index
//		_currentFrameMem = perDrawSratchMem->GetWritable(sizeof(GPUViewConstants), currentFrame);
//
//		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, ViewMatrix), _view.GetCameraMatrix());
//		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, ViewProjectionMatrix), _view.GetViewProjMatrix());
//		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, InvViewProjectionMatrix), _view.GetInvViewProjMatrix());
//		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, ViewPosition), _view.GetCameraPosition());
//
//		// FIXME
//		HACKS_CameraPos = _view.GetCameraPosition();
//
//		for (int32_t Iter = 0; Iter < ARRAY_SIZE(frustumPlanes); Iter++)
//		{
//			WriteMem(_currentFrameMem, offsetof(GPUViewConstants, FrustumPlanes[Iter]), frustumPlanes[Iter].coeffs());
//		}
//
//		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, RecipTanHalfFovy), _view.GetRecipTanHalfFovy());
//			
//
//#if 0
//		for (auto renderItem : _renderables)
//		{
//			renderItem->DrawDebug(_lines);
//		}
//		DrawDebug();
//#endif
//
//		if (_skyBox)
//		{
//			DrawSkyBox();
//		}
//
//#if 1
//		_octree.WalkElements(frustumPlanes, [](const IOctreeElement* InElement) -> bool
//			{
//				((Renderable*)InElement)->Draw();
//				return true;
//			});
//#else
//		for (auto renderItem : _renderables)
//		{
//			renderItem->Draw();
//		}
//#endif
	};

	//void VulkanRenderScene::DrawSkyBox()
	//{
		//auto pd3dDevice = GGraphicsDevice->GetDevice();
		//auto perDrawDescriptorHeap = GGraphicsDevice->GetDynamicDescriptorHeap();
		//auto perDrawSamplerHeap = GGraphicsDevice->GetDynamicSamplerHeap();
		//auto perDrawSratchMem = GGraphicsDevice->GetPerFrameScratchMemory();
		//auto cmdList = GGraphicsDevice->GetCommandList();
		//auto currentFrame = GGraphicsDevice->GetFrameCount();

		//ID3D12RootSignature* rootSig = nullptr;

		//if (_fullscreenRayVS)
		//{
		//	rootSig = _fullscreenRayVS->GetAs<D3D12Shader>().GetRootSignature();
		//}

		//cmdList->SetGraphicsRootSignature(rootSig);

		////table 0, shared all constant, scene stuff 
		//{
		//	cmdList->SetGraphicsRootConstantBufferView(0, GetGPUAddrOfViewConstants());

		//	CD3DX12_VIEWPORT m_viewport(0.0f, 0.0f, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
		//	CD3DX12_RECT m_scissorRect(0, 0, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
		//	cmdList->RSSetViewports(1, &m_viewport);
		//	cmdList->RSSetScissorRects(1, &m_scissorRect);
		//}

		////table 1, VS only constants
		//{
		//	auto pd3dDevice = GGraphicsDevice->GetDevice();

		//	_declspec(align(256u))
		//		struct GPUDrawConstants
		//	{
		//		//altered viewposition translated
		//		Matrix4x4 LocalToWorldScaleRotation;
		//		Vector3d Translation;
		//	};

		//	Matrix4x4 matId = Matrix4x4::Identity();
		//	Vector3d dummyVec(0, 0, 0);

		//	// write local to world
		//	auto HeapAddrs = perDrawSratchMem->GetWritable(sizeof(GPUDrawConstants), currentFrame);
		//	WriteMem(HeapAddrs, offsetof(GPUDrawConstants, LocalToWorldScaleRotation), matId);
		//	WriteMem(HeapAddrs, offsetof(GPUDrawConstants, Translation), dummyVec);

		//	cmdList->SetGraphicsRootConstantBufferView(1, HeapAddrs.gpuAddr);
		//}

		//cmdList->SetPipelineState(_fullscreenSkyBoxPSO->GetState());
		//cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		//auto SRVSlotBlock = perDrawDescriptorHeap->GetDescriptorSlots(1);
		//auto SamplerSlotBlock = perDrawSamplerHeap->GetDescriptorSlots(1);

		//{
		//	auto psSRVDescriptor = SRVSlotBlock[0];
		//	auto texRef = _skyBox->GetAs<D3D12Texture>();
		//	pd3dDevice->CopyDescriptorsSimple(1,
		//		psSRVDescriptor.cpuHandle,
		//		texRef.GetCPUDescriptor()->GetCPUDescriptorHandleForHeapStart(),
		//		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		//}
	
		//{
		//	auto psSamplerDescriptor = SamplerSlotBlock[0];

		//	D3D12_SAMPLER_DESC wrapSamplerDesc = {};
		//	wrapSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		//	wrapSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		//	wrapSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		//	wrapSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		//	wrapSamplerDesc.MinLOD = 0;
		//	wrapSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		//	wrapSamplerDesc.MipLODBias = 0.0f;
		//	wrapSamplerDesc.MaxAnisotropy = 1;
		//	wrapSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		//	wrapSamplerDesc.BorderColor[0] = wrapSamplerDesc.BorderColor[1] = wrapSamplerDesc.BorderColor[2] = wrapSamplerDesc.BorderColor[3] = 0;

		//	pd3dDevice->CreateSampler(&wrapSamplerDesc, psSamplerDescriptor.cpuHandle);
		//}

		//cmdList->SetGraphicsRootDescriptorTable(7, SRVSlotBlock.gpuHandle);
		//cmdList->SetGraphicsRootDescriptorTable(12, SamplerSlotBlock.gpuHandle);

		//cmdList->DrawInstanced(4, 1, 0, 0);
//	}
}