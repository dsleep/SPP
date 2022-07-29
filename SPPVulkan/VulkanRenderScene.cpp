// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"
#include "VulkanDevice.h"
#include "VulkanShaders.h"
#include "VulkanTexture.h"
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
	extern GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GraphicsDevice* InOwner, GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);

	static Vector3d HACKS_CameraPos;

	class GlobalVulkanRenderSceneResources : public GlobalGraphicsResource
	{
	private:
		GPUReferencer < GPUShader > _fullscreenColorVS, _fullscreenColorPS;
		GPUReferencer< PipelineState > _fullscreenColorPSO;
		GPUReferencer< GPUInputLayout > _fullscreenColorLayout;

	public:
		// called on render thread
		virtual void Initialize(class GraphicsDevice* InOwner)
		{
			_fullscreenColorVS = Make_GPU(VulkanShader, InOwner, EShaderType::Vertex);  
			_fullscreenColorVS->CompileShaderFromFile("shaders/fullScreenColorWrite.hlsl", "main_vs");

			_fullscreenColorPS = Make_GPU(VulkanShader, InOwner, EShaderType::Pixel);
			_fullscreenColorPS->CompileShaderFromFile("shaders/fullScreenColorWrite.hlsl", "main_ps");

			_fullscreenColorLayout = Make_GPU(VulkanInputLayout, InOwner);

			{
				auto& vulkanInputLayout = _fullscreenColorLayout->GetAs<VulkanInputLayout>();
				vulkanInputLayout.InitializeLayout(std::vector<VertexStream>());
			}


			auto backbufferFrameData = GGlobalVulkanGI->GetBackBufferFrameData();

			auto vulkPSO = Make_GPU(VulkanPipelineState,InOwner);
			vulkPSO->ManualSetRenderPass(backbufferFrameData.renderPass);
			_fullscreenColorPSO = vulkPSO;
			vulkPSO->Initialize(EBlendState::Disabled,
				ERasterizerState::NoCull,
				EDepthState::Enabled,
				EDrawingTopology::TriangleStrip,
				_fullscreenColorLayout,
				_fullscreenColorVS,
				_fullscreenColorPS,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr);
		}

		auto GetFullScreenWritePSO()
		{
			return _fullscreenColorPSO;
		}

		virtual void Shutdown(class GraphicsDevice* InOwner)
		{
			_fullscreenColorVS.Reset();
			_fullscreenColorPS.Reset();
			_fullscreenColorPSO.Reset();
			_fullscreenColorLayout.Reset();
		}
	};

	GlobalVulkanRenderSceneResources GVulkanSceneResrouces;
	

	VulkanRenderScene::VulkanRenderScene(GraphicsDevice* InOwner) : RT_RenderScene(InOwner)
	{
		//SE_ASSERT(I());

		_defaultMaterial = _owner->CreateMaterial();
		
		_meshvertexShader = _owner->CreateShader();
		_meshpixelShader = _owner->CreateShader();

		_debugDrawer = std::make_unique< VulkanDebugDrawing >(_owner);
	}

	VulkanRenderScene::~VulkanRenderScene()
	{

	}

	void VulkanRenderScene::AddedToGraphicsDevice()
	{
		_defaultMaterial->SetMaterialArgs({ .vertexShader = _meshvertexShader, .pixelShader = _meshpixelShader });

		_meshvertexShader->Initialize(EShaderType::Vertex);
		_meshvertexShader->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_vs");
		_meshpixelShader->Initialize(EShaderType::Pixel);
		_meshpixelShader->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_ps");
			
		_debugDrawer->Initialize();

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
		_fullscreenRayVS = Make_GPU(VulkanShader, _owner, EShaderType::Vertex);
		_fullscreenRayVS->CompileShaderFromFile("shaders/fullScreenRayVS.hlsl", "main_vs");

		_fullscreenRaySDFPS = Make_GPU(VulkanShader, _owner, EShaderType::Pixel);
		_fullscreenRaySDFPS->CompileShaderFromFile("shaders/fullScreenRaySDFPS.hlsl", "main_ps");

		//_fullscreenRaySkyBoxPS = Vulkan_CreateShader(EShaderType::Pixel);
		//_fullscreenRaySkyBoxPS->CompileShaderFromFile("shaders/fullScreenRayCubemapPS.hlsl", "main_ps");

		_fullscreenRayVSLayout = Make_GPU(VulkanInputLayout, _owner); 

		{
			auto& vulkanInputLayout = _fullscreenRayVSLayout->GetAs<VulkanInputLayout>();
			vulkanInputLayout.InitializeLayout(std::vector<VertexStream>());
		}

		_fullscreenRaySDFPSO = GetVulkanPipelineState(_owner,
			EBlendState::Disabled,
			ERasterizerState::NoCull,
			EDepthState::Enabled,
			EDrawingTopology::TriangleStrip,
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
		_cameraBuffer = Vulkan_CreateStaticBuffer(_owner, GPUBufferType::Simple, _cameraData);

		_drawConstants = std::make_shared< ArrayResource >();
		_drawConstants->InitializeFromType< GPUDrawConstants >(InFlightFrames);
		_drawConstantsBuffer = Vulkan_CreateStaticBuffer(_owner, GPUBufferType::Simple, _drawConstants);

		_drawParams = std::make_shared< ArrayResource >();
		_drawParams->InitializeFromType< GPUDrawParams >(InFlightFrames);
		_drawParamsBuffer = Vulkan_CreateStaticBuffer(_owner, GPUBufferType::Simple, _drawParams);

		_shapes = std::make_shared< ArrayResource >();
		_shapes->InitializeFromType< SDFShape >(InFlightFrames);
		_shapesBuffer = Vulkan_CreateStaticBuffer(_owner, GPUBufferType::Array, _drawParams);


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
				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),

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

	void VulkanRenderScene::AddDebugLine(const Vector3d& Start, const Vector3d& End, const Vector3& Color)
	{
		_debugDrawer->AddDebugLine(Start, End, Color);
	}
	void VulkanRenderScene::AddDebugBox(const Vector3d& Center, const Vector3d& Extents, const Vector3& Color)
	{
		_debugDrawer->AddDebugBox(Center, Extents, Color);
	}
	void VulkanRenderScene::AddDebugSphere(const Vector3d& Center, float Radius, const Vector3& Color)
	{
		_debugDrawer->AddDebugSphere(Center, Radius, Color);
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


	void VulkanRenderScene::AddRenderable(Renderable* InRenderable)
	{
		RT_RenderScene::AddRenderable(InRenderable);
	}

	void VulkanRenderScene::RemoveRenderable(Renderable* InRenderable)
	{
		RT_RenderScene::RemoveRenderable(InRenderable);
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

	std::shared_ptr< class RT_RenderScene > VulkanGraphicsDevice::CreateRenderScene()
	{
		return std::make_shared<VulkanRenderScene>(this);
	}


	void VulkanRenderScene::BeginFrame()
	{
		for (auto renderItem : _renderables3d)
		{
			renderItem->PrepareToDraw();
		}
		for (auto renderItem : _renderablesPost)
		{
			renderItem->PrepareToDraw();
		}

		_debugDrawer->PrepareForDraw();

		extern VkDevice GGlobalVulkanDevice;
		extern VulkanGraphicsDevice* GGlobalVulkanGI;

		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		//UPDATE UNIFORMS
		//_viewGPU.GenerateLeftHandFoVPerspectiveMatrix(75.0f, (float)DeviceExtents[0] / (float)DeviceExtents[1]);
		_viewGPU.BuildCameraMatrices();

		Planed frustumPlanes[6];
		_viewGPU.GetFrustumPlanes(frustumPlanes);

		auto cameraSpan = _cameraData->GetSpan< GPUViewConstants>();
		GPUViewConstants& curCam = cameraSpan[currentFrame];
		curCam.ViewMatrix = _viewGPU.GetCameraMatrix();
		curCam.ViewProjectionMatrix = _viewGPU.GetViewProjMatrix();
		curCam.InvViewProjectionMatrix = _viewGPU.GetInvViewProjMatrix();
		curCam.InvProjectionMatrix = _viewGPU.GetInvProjectionMatrix();
		curCam.ViewPosition = _viewGPU.GetCameraPosition();
		curCam.FrameExtents = DeviceExtents;

		for (int32_t Iter = 0; Iter < ARRAY_SIZE(frustumPlanes); Iter++)
		{
			curCam.FrustumPlanes[Iter] = frustumPlanes[Iter].coeffs();
		}

		curCam.RecipTanHalfFovy = _viewGPU.GetRecipTanHalfFovy();
		_cameraBuffer->UpdateDirtyRegion(currentFrame, 1);

		{
			auto uniformData = _drawConstants->GetSpan< GPUDrawConstants>();
			auto& curData = uniformData[currentFrame];
			curData.LocalToWorldScaleRotation = Matrix4x4::Identity();
			curData.Translation = Vector3d(0, 0, 0);
			_drawConstantsBuffer->UpdateDirtyRegion(currentFrame, 1);
		}

		{
			auto uniformData = _drawParams->GetSpan<GPUDrawParams>();
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
	}

	void VulkanRenderScene::Draw()
	{
		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		auto &camPos = _viewGPU.GetCameraPosition();
		std::string CameraText = std::string_format("CAMERA: %.1f %.1f %.1f", camPos[0], camPos[1], camPos[2]);
		GGlobalVulkanGI->DrawDebugText(Vector2i(10, 20), CameraText.c_str() );
		
		auto ColorTargetFrameData = GGlobalVulkanGI->GetColorFrameData();

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = ColorTargetFrameData.renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = DeviceExtents[0];
		renderPassBeginInfo.renderArea.extent.height = DeviceExtents[1];
		renderPassBeginInfo.clearValueCount = 2;
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 1.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassBeginInfo.pClearValues = clearValues;
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = ColorTargetFrameData.frameBuffer;

		// Start the first sub pass specified in our default render pass setup by the base class
		// This will clear the color and depth attachment
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.width = (float)DeviceExtents[0];
		viewport.height = (float)DeviceExtents[1];
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

#if 0
		for (auto renderItem : _renderables)
		{
			renderItem->DrawDebug(_lines);
		}
		DrawDebug();
#endif

		//if (_skyBox)
		{
			//DrawSkyBox();
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

		for (auto renderItem : _renderables3d)
		{
			renderItem->Draw();
		}

		_debugDrawer->Draw(this);

		vkCmdEndRenderPass(commandBuffer);


		auto &DepthColorTexture = GGlobalVulkanGI->GetDepthColor()->GetAs<VulkanTexture>();

		auto ColorTarget = GGlobalVulkanGI->GetColorTarget();
		auto& colorAttachment = ColorTarget->GetFrontAttachment();
		auto& depthAttachment = ColorTarget->GetBackAttachment();

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(vulkanDevice, depthAttachment.image->Get(), &memReqs);

		auto curDepthScratch = scratchBuffer.GetWritable(memReqs.size, currentFrame);

		vks::tools::setImageLayout(commandBuffer, depthAttachment.image->Get(),
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

		VkBufferImageCopy region = {};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent.width = DeviceExtents[0];
		region.imageExtent.height = DeviceExtents[1];
		region.imageExtent.depth = 1;
		region.bufferOffset = curDepthScratch.offsetFromBase;
		vkCmdCopyImageToBuffer(commandBuffer, 
			depthAttachment.image->Get(), 
			VK_IMAGE_LAYOUT_GENERAL,
			curDepthScratch.buffer, 
			1, 
			&region);

		vkGetImageMemoryRequirements(vulkanDevice, DepthColorTexture.GetVkImage(), &memReqs);
		SE_ASSERT(memReqs.size == curDepthScratch.size);

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		vkCmdCopyBufferToImage(
			commandBuffer,
			curDepthScratch.buffer,
			DepthColorTexture.GetVkImage(),
			VK_IMAGE_LAYOUT_GENERAL,
			1,
			&region
		);

		vks::tools::setImageLayout(commandBuffer, depthAttachment.image->Get(),
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });


		vks::tools::setImageLayout(commandBuffer, colorAttachment.image->Get(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		for (auto renderItem : _renderablesPost)
		{
			renderItem->Draw();
		}

		vks::tools::setImageLayout(commandBuffer, colorAttachment.image->Get(),
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		WriteToFrame();
	};

	void VulkanRenderScene::WriteToFrame()
	{
		extern VkDevice GGlobalVulkanDevice;
		extern VulkanGraphicsDevice* GGlobalVulkanGI;

		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		auto backbufferFrameData = GGlobalVulkanGI->GetBackBufferFrameData();
		auto FullScreenWritePSO = GVulkanSceneResrouces.GetFullScreenWritePSO();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = backbufferFrameData.renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = DeviceExtents[0];
		renderPassBeginInfo.renderArea.extent.height = DeviceExtents[1];
		renderPassBeginInfo.clearValueCount = 0;		
		renderPassBeginInfo.pClearValues = nullptr;
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = backbufferFrameData.frameBuffer;

		// Start the first sub pass specified in our default render pass setup by the base class
		// This will clear the color and depth attachment
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.width = (float)DeviceExtents[0];
		viewport.height = (float)DeviceExtents[1];
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

		//
		auto CurPool = GGlobalVulkanGI->GetActiveDescriptorPool();
		auto& curPSO = FullScreenWritePSO->GetAs<VulkanPipelineState>();
		auto& descriptorSetLayouts = curPSO.GetDescriptorSetLayouts();

		std::vector<VkDescriptorSet> locaDrawSets;
		locaDrawSets.resize(descriptorSetLayouts.size());

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(CurPool, descriptorSetLayouts.data(), descriptorSetLayouts.size());
		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice, &allocInfo, locaDrawSets.data()));

		VkDescriptorImageInfo textureInfo = GGlobalVulkanGI->GetColorImageDescImgInfo();	

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(locaDrawSets[0],
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0, &textureInfo),
				vks::initializers::writeDescriptorSet(locaDrawSets[0],
					VK_DESCRIPTOR_TYPE_SAMPLER, 1, &textureInfo),
		};

		vkUpdateDescriptorSets(vulkanDevice,
			static_cast<uint32_t>(writeDescriptorSets.size()),
			writeDescriptorSets.data(), 0, nullptr);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, curPSO.GetVkPipeline());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			curPSO.GetVkPipelineLayout(), 0, locaDrawSets.size(), locaDrawSets.data(), 0, nullptr);
		vkCmdDraw(commandBuffer, 4, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);
	}

	void VulkanRenderScene::DrawSkyBox()
	{
		extern VkDevice GGlobalVulkanDevice;
		extern VulkanGraphicsDevice* GGlobalVulkanGI;

		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		uint32_t uniform_offsets[] = {
			(sizeof(GPUViewConstants)) * currentFrame,
			(sizeof(GPUDrawConstants)) * currentFrame,
			(sizeof(GPUDrawParams)) * currentFrame,
			(sizeof(SDFShape)) * currentFrame,
		};

		auto& rayPSO = _fullscreenRaySDFPSO->GetAs<VulkanPipelineState>();
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayPSO.GetVkPipeline());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayPSO.GetVkPipelineLayout(), 0, 1,
			&_perDrawDescriptorSet, ARRAY_SIZE(uniform_offsets), uniform_offsets);
		vkCmdDraw(commandBuffer, 4, 1, 0, 0);
	}
}