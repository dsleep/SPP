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
	extern GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	extern GPUReferencer< GPUInputLayout > Vulkan_CreateInputLayout();

	static Vector3d HACKS_CameraPos;

	_declspec(align(256u)) struct GPUViewConstants
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

	_declspec(align(256u)) struct DrawConstants
	{
		//altered viewposition translated
		Matrix4x4 LocalToWorldScaleRotation;
		Vector3d Translation;
	};

	_declspec(align(256u)) struct DrawParams
	{
		//all origin centered
		Vector3 ShapeColor;
		uint32_t ShapeCount;
	};

	_declspec(align(256u)) struct SDFShape
	{
		Vector3  translation;
		Vector3  eulerRotation;
		Vector4  shapeBlendAndScale;
		Vector4  params;
		uint32_t shapeType;
		uint32_t shapeOp;
	};


	

	VulkanRenderScene::VulkanRenderScene()
	{
		//_debugVS = Vulkan_CreateShader(EShaderType::Vertex);
		//_debugVS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_vs");

		//_debugPS = Vulkan_CreateShader(EShaderType::Pixel);
		//_debugPS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_ps");


		//_debugLayout = Vulkan_CreateInputLayout();

		//{
		//	auto& vulkanInputLayout = _debugLayout->GetAs<VulkanInputLayout>();
		//	DebugVertex dvPattern;
		//	vulkanInputLayout.Begin();
		//	vulkanInputLayout.AddVertexStream(dvPattern, dvPattern.position, dvPattern.color);
		//	vulkanInputLayout.Finalize();
		//}
		//	
		////_debugLayout->InitializeLayout({
		////		{ "POSITION",  InputLayoutElementType::Float3, offsetof(DebugVertex,position) },
		////		{ "COLOR",  InputLayoutElementType::Float3, offsetof(DebugVertex,color) }
		////	});

		//_debugPSO = GetVulkanPipelineState(EBlendState::Disabled,
		//	ERasterizerState::NoCull,
		//	EDepthState::Enabled,
		//	EDrawingTopology::LineList,
		//	_debugLayout,
		//	_debugVS,
		//	_debugPS,
		//	nullptr,
		//	nullptr,
		//	nullptr,
		//	nullptr,
		//	nullptr);

		//_debugResource = std::make_shared< ArrayResource >();
		//_debugResource->InitializeFromType< DebugVertex >(10 * 1024);
		//_debugBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Vertex, _debugResource);

		//
		_fullscreenRayVS = Vulkan_CreateShader(EShaderType::Vertex);
		_fullscreenRayVS->CompileShaderFromFile("shaders/fullScreenRayVS.hlsl", "main_vs");

		_fullscreenRaySDFPS = Vulkan_CreateShader(EShaderType::Pixel);
		_fullscreenRaySDFPS->CompileShaderFromFile("shaders/fullScreenRaySDFPS.hlsl", "main_ps");

		//_fullscreenRaySkyBoxPS = Vulkan_CreateShader(EShaderType::Pixel);
		//_fullscreenRaySkyBoxPS->CompileShaderFromFile("shaders/fullScreenRayCubemapPS.hlsl", "main_ps");

		_fullscreenRayVSLayout = Vulkan_CreateInputLayout();

		{
			auto& vulkanInputLayout = _fullscreenRayVSLayout->GetAs<VulkanInputLayout>();
			FullscreenVertex dvPattern;
			vulkanInputLayout.Begin();
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

		//_fullscreenSkyBoxPSO = GetVulkanPipelineState(EBlendState::Disabled,
		//	ERasterizerState::NoCull,
		//	EDepthState::Disabled,
		//	EDrawingTopology::TriangleList,
		//	_fullscreenRayVSLayout,
		//	_fullscreenRayVS,
		//	_fullscreenRaySkyBoxPS,
		//	nullptr,
		//	nullptr,
		//	nullptr,
		//	nullptr,
		//	nullptr);


		//
		extern VulkanGraphicsDevice* GGlobalVulkanGI;

		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		const auto InFlightFrames = GGlobalVulkanGI->GetInFlightFrames();
			
		_cameraData = std::make_shared< ArrayResource >();
		_cameraData->InitializeFromType< GPUViewConstants >(InFlightFrames);
		_cameraBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Simple, _cameraData);

		_drawConstants = std::make_shared< ArrayResource >();
		_drawConstants->InitializeFromType< DrawConstants >(InFlightFrames);
		_drawConstantsBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Simple, _drawConstants);

		_drawParams = std::make_shared< ArrayResource >();
		_drawParams->InitializeFromType< DrawParams >(InFlightFrames);
		_drawParamsBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Simple, _drawParams);

		_shapes = std::make_shared< ArrayResource >();
		_shapes->InitializeFromType< SDFShape >(InFlightFrames);
		_shapesBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Array, _drawParams);


		//

		//PER FRAME DESCRIPTOR SET (1 set using VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		//{
		//	std::vector<VkDescriptorSetLayoutBinding> descriptSetLayout = {
		//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_ALL_GRAPHICS, 0)
		//	};
		//	auto layoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(descriptSetLayout.data(), static_cast<uint32_t>(descriptSetLayout.size()));

		//	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice, &layoutCreateInfo, nullptr, &_perFrameSetLayout));

		//	//
		//	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(_descriptorPool, &_perFrameSetLayout, 1);
		//	VkResult allocateCall = vkAllocateDescriptorSets(vulkanDevice, &allocInfo, &_perFrameDescriptorSet);

		//	VkDescriptorBufferInfo perFrameInfo;
		//	perFrameInfo.buffer = _cameraBuffer->GetBuffer();
		//	perFrameInfo.offset = 0;
		//	perFrameInfo.range = _cameraBuffer->GetPerElementSize();

		//	VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(_perFrameDescriptorSet,
		//		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo);
		//	vkUpdateDescriptorSets(vulkanDevice, 1, &writeDescriptorSet, 0, nullptr);
		//}


		std::vector<VkDescriptorPoolSize> mainPool = {

			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 32)
		};

		auto poolCreateInfo = vks::initializers::descriptorPoolCreateInfo(mainPool, 2);
		poolCreateInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
		VK_CHECK_RESULT(vkCreateDescriptorPool(vulkanDevice, &poolCreateInfo, nullptr, &_descriptorPool));


		//PER DRAW DESCRIPTOR SET
		{
			std::vector<VkDescriptorSetLayoutBinding> descriptSetLayout = {
				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_ALL_GRAPHICS, 0), 

				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT, 2),

				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT, 3)
			};
			auto layoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(descriptSetLayout.data(), static_cast<uint32_t>(descriptSetLayout.size()));

			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice, &layoutCreateInfo, nullptr, &_perDrawSetLayout));

			//
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(_descriptorPool, &_perDrawSetLayout, 1);
			VkResult allocateCall = vkAllocateDescriptorSets(vulkanDevice, &allocInfo, &_perDrawDescriptorSet);

			VkDescriptorBufferInfo perFrameInfo;
			perFrameInfo.buffer = _cameraBuffer->GetBuffer();
			perFrameInfo.offset = 0;
			perFrameInfo.range = _cameraBuffer->GetPerElementSize();

			VkDescriptorBufferInfo drawConstsInfo;
			drawConstsInfo.buffer = _drawConstantsBuffer->GetBuffer();
			drawConstsInfo.offset = 0;
			drawConstsInfo.range = _drawConstantsBuffer->GetPerElementSize();

			VkDescriptorBufferInfo drawParamsInfo;
			drawParamsInfo.buffer = _drawParamsBuffer->GetBuffer();
			drawParamsInfo.offset = 0;
			drawParamsInfo.range = _drawParamsBuffer->GetPerElementSize();

			VkDescriptorBufferInfo shapesInfo;
			shapesInfo.buffer = _shapesBuffer->GetBuffer();
			shapesInfo.offset = 0;
			shapesInfo.range = _shapesBuffer->GetPerElementSize();

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(_perDrawDescriptorSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
				vks::initializers::writeDescriptorSet(_perDrawDescriptorSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &drawConstsInfo),
				vks::initializers::writeDescriptorSet(_perDrawDescriptorSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2, &drawParamsInfo),
				vks::initializers::writeDescriptorSet(_perDrawDescriptorSet,
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 3, &shapesInfo)
			};

			vkUpdateDescriptorSets(vulkanDevice,
				static_cast<uint32_t>(writeDescriptorSets.size()), 
				writeDescriptorSets.data(), 0, nullptr);
		}
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
	}

	void VulkanRenderScene::RemoveFromScene(Renderable* InRenderable)
	{
		RenderScene::RemoveFromScene(InRenderable);
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

		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		//UPDATE UNIFORMS
		_view.GenerateLeftHandFoVPerspectiveMatrix(45.0f, (float)DeviceExtents[0] / (float)DeviceExtents[1]);
		_view.BuildCameraMatrices();

		Planed frustumPlanes[6];
		_view.GetFrustumPlanes(frustumPlanes);

		auto cameraSpan = _cameraData->GetSpan< GPUViewConstants>();
		GPUViewConstants& curCam = cameraSpan[currentFrame];
		curCam.ViewMatrix = _view.GetCameraMatrix();
		curCam.ViewProjectionMatrix = _view.GetViewProjMatrix();
		curCam.InvViewProjectionMatrix = _view.GetInvViewProjMatrix();
		curCam.ViewPosition = _view.GetCameraPosition();

		for (int32_t Iter = 0; Iter < ARRAY_SIZE(frustumPlanes); Iter++)
		{
			curCam.FrustumPlanes[Iter] = frustumPlanes[Iter].coeffs();
		}

		curCam.RecipTanHalfFovy = _view.GetRecipTanHalfFovy();
		_cameraBuffer->UpdateDirtyRegion(currentFrame, 1);

		{
			auto uniformData = _drawConstants->GetSpan< DrawConstants>();
			auto& curData = uniformData[currentFrame];
			curData.LocalToWorldScaleRotation = Matrix4x4::Identity();
			curData.Translation = Vector3d(0, 0, 0);
			_drawConstantsBuffer->UpdateDirtyRegion(currentFrame, 1);
		}

		{
			auto uniformData = _drawParams->GetSpan<DrawParams>();
			auto& curData = uniformData[currentFrame];
			curData.ShapeCount = 0;
			curData.ShapeColor = Vector3(0, 0, 0);
			_drawParamsBuffer->UpdateDirtyRegion(currentFrame, 1);
		}

		{
			auto uniformData = _shapes->GetSpan<SDFShape>();
			auto& curData = uniformData[currentFrame];
			memset(&curData, 0, sizeof(curData));
			_shapesBuffer->UpdateDirtyRegion(currentFrame, 1);
		}

		static float color = 0.0f;

		// Set clear values for all framebuffer attachments with loadOp set to clear
		// We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, color, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = basicRenderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = DeviceExtents[0];
		renderPassBeginInfo.renderArea.extent.height = DeviceExtents[1];
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = GGlobalVulkanGI->GetActiveFrameBuffer();

		// Start the first sub pass specified in our default render pass setup by the base class
		// This will clear the color and depth attachment
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.height = (float)DeviceExtents[0];
		viewport.width = (float)DeviceExtents[1];
		viewport.minDepth = (float)0.0f;
		viewport.maxDepth = (float)1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// Update dynamic scissor state
		VkRect2D scissor = {};
		scissor.extent.width = DeviceExtents[0];
		scissor.extent.height = DeviceExtents[1];
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
				
		// Bind scene matrices descriptor to set 0
		//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

#if 0
		for (auto renderItem : _renderables)
		{
			renderItem->DrawDebug(_lines);
		}
		DrawDebug();
#endif

		//if (_skyBox)
		{
			DrawSkyBox();
		}

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

	void VulkanRenderScene::DrawSkyBox()
	{
		extern VkDevice GGlobalVulkanDevice;
		extern VulkanGraphicsDevice* GGlobalVulkanGI;

		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		
		////if (_fullscreenRayVS)
		////{
		////	rootSig = _fullscreenRayVS->GetAs<D3D12Shader>().GetRootSignature();
		////}

		uint32_t uniform_offsets[]  = {
			(sizeof(GPUViewConstants)) * currentFrame,
			(sizeof(DrawConstants))* currentFrame,
			(sizeof(DrawParams))* currentFrame,
			(sizeof(SDFShape))* currentFrame,
		};
		auto &rayPSO = _fullscreenRaySDFPSO->GetAs<VulkanPipelineState>();
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayPSO.GetVkPipeline());
		//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayPSO.GetVkPipelineLayout(), 0, 1,
			//&_perFrameDescriptorSet, 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayPSO.GetVkPipelineLayout(), 0, 1,
			&_perDrawDescriptorSet, ARRAY_SIZE(uniform_offsets), uniform_offsets);
		vkCmdDrawIndexed(commandBuffer, 4, 1, 0, 0, 0);
	}
}