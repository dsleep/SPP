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


namespace std
{		
	template<>
	struct hash<SPP::MaterialKey>
	{
		std::size_t operator()(const SPP::MaterialKey& InValue) const noexcept
		{
			return InValue.Hash();
		}
	};
}

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
			vulkPSO->Initialize(EBlendState::AlphaBlend,
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

	class GlobalOpaqueDrawerResources : public GlobalGraphicsResource
	{
	private:
		GPUReferencer < VulkanShader > _opaqueVS, _opaquePS, _opaquePSWithLightMap, _opaqueVSWithLightMap;
		GPUReferencer< SafeVkDescriptorSetLayout > _opaqueVSLayout;

	public:
		// called on render thread
		virtual void Initialize(class GraphicsDevice* InOwner)
		{
			auto owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InOwner);

			_opaqueVS = Make_GPU(VulkanShader, InOwner, EShaderType::Vertex);
			_opaquePS = Make_GPU(VulkanShader, InOwner, EShaderType::Pixel);
			_opaqueVSWithLightMap = Make_GPU(VulkanShader, InOwner, EShaderType::Vertex);
			_opaquePSWithLightMap = Make_GPU(VulkanShader, InOwner, EShaderType::Pixel);

			_opaqueVS->CompileShaderFromFile("shaders/SimpleTextureMesh.hlsl", "main_vs");
			_opaquePS->CompileShaderFromFile("shaders/SimpleTextureMesh.hlsl", "main_ps");
			_opaqueVSWithLightMap->CompileShaderFromFile("shaders/SimpleTextureLightMapMesh.hlsl", "main_vs");
			_opaquePSWithLightMap->CompileShaderFromFile("shaders/SimpleTextureLightMapMesh.hlsl", "main_ps");

			auto& vsSet = _opaqueVS->GetLayoutSets();
			_opaqueVSLayout = Make_GPU(SafeVkDescriptorSetLayout, owningDevice, vsSet.front().bindings);
		}

		GPUReferencer< SafeVkDescriptorSetLayout > GetOpaqueVSLayout()
		{
			return _opaqueVSLayout;
		}

		virtual void Shutdown(class GraphicsDevice* InOwner)
		{
			_opaqueVS.Reset();
			_opaquePS.Reset();
			_opaqueVSWithLightMap.Reset();
			_opaquePSWithLightMap.Reset();
		}
	};

	GlobalVulkanRenderSceneResources GVulkanSceneResrouces;
	GlobalOpaqueDrawerResources GVulkanOpaqueResrouces;
		
	class OpaqueDrawer
	{
	protected:
		GPUReferencer< SafeVkDescriptorSet > _camStaticBufferDescriptorSet;
		VulkanGraphicsDevice* _owningDevice = nullptr;
		VulkanRenderScene* _owningScene = nullptr;

		std::unordered_map< MaterialKey, GPUReferencer< SafeVkDescriptorSet > > _materialDescriptorCache;

	public:
		OpaqueDrawer(VulkanRenderScene *InScene) : _owningScene(InScene)
		{
			_owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InScene->GetOwner());
			auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();

			auto meshVSLayout = GVulkanOpaqueResrouces.GetOpaqueVSLayout();
			_camStaticBufferDescriptorSet = Make_GPU(SafeVkDescriptorSet, _owningDevice, meshVSLayout->Get(), globalSharedPool);

			auto cameraBuffer = InScene->GetCameraBuffer();

			VkDescriptorBufferInfo perFrameInfo;
			perFrameInfo.buffer = cameraBuffer->GetBuffer();
			perFrameInfo.offset = 0;
			perFrameInfo.range = cameraBuffer->GetPerElementSize();

			VkDescriptorBufferInfo drawConstsInfo;
			auto staticDrawBuffer = GGlobalVulkanGI->GetStaticInstanceDrawBuffer();
			drawConstsInfo.buffer = staticDrawBuffer->GetBuffer();
			drawConstsInfo.offset = 0;
			drawConstsInfo.range = staticDrawBuffer->GetPerElementSize();

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(_camStaticBufferDescriptorSet->Get(),
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
				vks::initializers::writeDescriptorSet(_camStaticBufferDescriptorSet->Get(),
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &drawConstsInfo),
			};

			vkUpdateDescriptorSets(_owningDevice->GetDevice(),
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);
		}

		GPUReferencer< SafeVkDescriptorSet > GetMaterialDescriptorSet(VkDescriptorSetLayout InSetLayout,
			const std::vector<VkDescriptorSetLayoutBinding> &textureBindings, 
			std::shared_ptr<RT_Material> InMat)
		{
			MaterialKey matKey(InMat);

			auto findCachedItem = _materialDescriptorCache.find(matKey);
			
			if (findCachedItem != _materialDescriptorCache.end())
			{
				return findCachedItem->second;
			}
			else
			{
				VkDescriptorImageInfo textureInfo[4];

				auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();
				std::vector<VkWriteDescriptorSet> writeDescriptorSets;
				int32_t TextureCount = textureBindings.size() / 2;
				auto newTextureDescSet = Make_GPU(SafeVkDescriptorSet, _owningDevice, InSetLayout, globalSharedPool);

				auto& textureMap = InMat->GetTextureMap();
				for (int32_t Iter = 0; Iter < TextureCount; Iter++)
				{
					auto getTexture = textureMap.find((TexturePurpose)Iter);

					auto gpuTexture = getTexture != textureMap.end() ?
						getTexture->second->GetGPUTexture() :
						GGlobalVulkanGI->GetDefaultTexture();

					auto& currentVulkanTexture = gpuTexture->GetAs<VulkanTexture>();
					textureInfo[Iter] = currentVulkanTexture.GetDescriptor();

					writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(newTextureDescSet->Get(),
						VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, (Iter * 2) + 0, &textureInfo[Iter]));
					writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(newTextureDescSet->Get(),
						VK_DESCRIPTOR_TYPE_SAMPLER, (Iter * 2) + 1, &textureInfo[Iter]));
				}

				vkUpdateDescriptorSets(_owningDevice->GetDevice(),
					static_cast<uint32_t>(writeDescriptorSets.size()),
					writeDescriptorSets.data(), 0, nullptr);

				_materialDescriptorCache[matKey] = newTextureDescSet;
			}


		}

		void Render()
		{

		}
	};

	

	VulkanRenderScene::VulkanRenderScene(GraphicsDevice* InOwner) : RT_RenderScene(InOwner)
	{
		//SE_ASSERT(I());
		
		_debugDrawer = std::make_unique< VulkanDebugDrawing >(_owner);
	}

	VulkanRenderScene::~VulkanRenderScene()
	{

	}

	void VulkanRenderScene::AddedToGraphicsDevice()
	{
		//_defaultMaterial->SetMaterialArgs({ .vertexShader = _meshvertexShader, .pixelShader = _meshpixelShader });
/*
		_meshvertexShader->Initialize(EShaderType::Vertex);
		_meshvertexShader->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_vs");
		_meshpixelShader->Initialize(EShaderType::Pixel);
		_meshpixelShader->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_ps");
	*/		
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


	void VulkanRenderScene::AddRenderable(Renderable* InRenderable)
	{
		RT_RenderScene::AddRenderable(InRenderable);
	}

	void VulkanRenderScene::RemoveRenderable(Renderable* InRenderable)
	{
		RT_RenderScene::RemoveRenderable(InRenderable);
	}

	std::shared_ptr< class RT_RenderScene > VulkanGraphicsDevice::CreateRenderScene()
	{
		return std::make_shared<VulkanRenderScene>(this);
	}

	void VulkanRenderScene::BeginFrame()
	{
		RT_RenderScene::BeginFrame();

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

		_viewGPU.GetFrustumPlanes(_frustumPlanes);

		auto cameraSpan = _cameraData->GetSpan< GPUViewConstants>();
		GPUViewConstants& curCam = cameraSpan[currentFrame];
		curCam.ViewMatrix = _viewGPU.GetCameraMatrix();
		curCam.ViewProjectionMatrix = _viewGPU.GetViewProjMatrix();
		curCam.InvViewProjectionMatrix = _viewGPU.GetInvViewProjMatrix();
		curCam.InvProjectionMatrix = _viewGPU.GetInvProjectionMatrix();
		curCam.ViewPosition = _viewGPU.GetCameraPosition();
		curCam.FrameExtents = DeviceExtents;

		for (int32_t Iter = 0; Iter < ARRAY_SIZE(_frustumPlanes); Iter++)
		{
			curCam.FrustumPlanes[Iter] = _frustumPlanes[Iter].coeffs();
		}

		curCam.RecipTanHalfFovy = _viewGPU.GetRecipTanHalfFovy();
		_cameraBuffer->UpdateDirtyRegion(currentFrame, 1);
	}
		
	void VulkanRenderScene::OpaqueDepthPass()
	{
	}

	void VulkanRenderScene::OpaquePass()
	{
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

		int32_t curVisible = 0;
		_visible3d.resize(_renderables3d.size());

		_opaques.resize(_renderables3d.size());
		_translucents.resize(_renderables3d.size());

		#if 1
			_octree.WalkElements(_frustumPlanes, [&](const IOctreeElement* InElement) -> bool
				{
					auto curRenderable = ((Renderable*)InElement);
					_visible3d[curVisible++] = curRenderable;
					auto& drawInfo = curRenderable->GetDrawingInfo();

					//drawInfo.drawingType == DrawingType::Opaque
					((Renderable*)InElement)->Draw();
					return true;
				});
		#else
			for (auto renderItem : _renderables3d)
			{
				renderItem->Draw();
			}
		#endif

		
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
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			curDepthScratch.buffer, 
			1, 
			&region);

		vkGetImageMemoryRequirements(vulkanDevice, DepthColorTexture.GetVkImage(), &memReqs);
		SE_ASSERT(memReqs.size == curDepthScratch.size);

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;


		vks::tools::setImageLayout(commandBuffer, DepthColorTexture.GetVkImage(),
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		vkCmdCopyBufferToImage(
			commandBuffer,
			curDepthScratch.buffer,
			DepthColorTexture.GetVkImage(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		vks::tools::setImageLayout(commandBuffer, DepthColorTexture.GetVkImage(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		vks::tools::setImageLayout(commandBuffer, depthAttachment.image->Get(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
		auto CurPool = GGlobalVulkanGI->GetPerFrameResetDescriptorPool();
		auto& curPSO = FullScreenWritePSO->GetAs<VulkanPipelineState>();
		auto& descriptorSetLayouts = curPSO.GetDescriptorSetLayouts();

		// main scene to back buffer
		{
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
		}

		// ui to back buffer
		if(_offscreenUI)
		{
			std::vector<VkDescriptorSet> locaDrawSets;
			locaDrawSets.resize(descriptorSetLayouts.size());

			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(CurPool, descriptorSetLayouts.data(), descriptorSetLayouts.size());
			VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice, &allocInfo, locaDrawSets.data()));

			VkDescriptorImageInfo textureInfo = _offscreenUI->GetAs<VulkanTexture>().GetDescriptor();

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
		}

		vkCmdEndRenderPass(commandBuffer);
	}

	void VulkanRenderScene::DrawSkyBox()
	{
		//extern VkDevice GGlobalVulkanDevice;
		//extern VulkanGraphicsDevice* GGlobalVulkanGI;

		//auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		//auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		//auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		//auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		//auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		//uint32_t uniform_offsets[] = {
		//	(sizeof(GPUViewConstants)) * currentFrame,
		//	(sizeof(GPUDrawConstants)) * currentFrame,
		//	(sizeof(GPUDrawParams)) * currentFrame,
		//	(sizeof(SDFShape)) * currentFrame,
		//};

		//auto& rayPSO = _fullscreenRaySDFPSO->GetAs<VulkanPipelineState>();
		//vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayPSO.GetVkPipeline());
		//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayPSO.GetVkPipelineLayout(), 0, 1,
		//	&_perDrawDescriptorSet, ARRAY_SIZE(uniform_offsets), uniform_offsets);
		//vkCmdDraw(commandBuffer, 4, 1, 0, 0);
	}
}