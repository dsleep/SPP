// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"
#include "VulkanDevice.h"
#include "VulkanShaders.h"
#include "VulkanTexture.h"
#include "VulkanRenderableMesh.h"

#include "VulkanDeferredLighting.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"

namespace SPP
{
	extern LogEntry LOG_VULKAN;

	
	const std::vector<VertexStream>& OP_GetVertexStreams_DeferredLightingShapes()
	{
		static std::vector<VertexStream> vertexStreams;
		if (vertexStreams.empty())
		{
			MeshVertex dummy;
			vertexStreams.push_back(
				CreateVertexStream(dummy,
					dummy.position));
		}
		return vertexStreams;
	}

	class GlobalDeferredLightingResources : public GlobalGraphicsResource
	{
	private:
		GPUReferencer < VulkanShader > _lightShapeVS, _lightFullscreenVS, _lightSunPS;
		GPUReferencer< GPUInputLayout > _lightShapeLayout;

		GPUReferencer< VulkanPipelineState > _psoSunLight;
		GPUReferencer< SafeVkDescriptorSetLayout > _lightShapeVSLayout;

		std::map< ParameterMapKey, GPUReferencer < VulkanShader > > _psShaderMap;

	public:
		// called on render thread
		virtual void Initialize(class GraphicsDevice* InOwner)
		{
			auto owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InOwner);

			_lightShapeVS = Make_GPU(VulkanShader, InOwner, EShaderType::Vertex);
			_lightShapeVS->CompileShaderFromFile("shaders/Deferred/LightShapeVS.glsl");

			_lightFullscreenVS = Make_GPU(VulkanShader, InOwner, EShaderType::Vertex);
			_lightFullscreenVS->CompileShaderFromFile("shaders/Deferred/FullScreenLightVS.glsl");

			_lightSunPS = Make_GPU(VulkanShader, InOwner, EShaderType::Pixel);
			_lightSunPS->CompileShaderFromFile("shaders/Deferred/SunLightPS.glsl");

			_lightShapeLayout = Make_GPU(VulkanInputLayout, InOwner);
			_lightShapeLayout->InitializeLayout(OP_GetVertexStreams_DeferredLightingShapes());

			_psoSunLight = GetVulkanPipelineState(owningDevice,
				owningDevice->GetLightingCompositeRenderPass(),
				EBlendState::Additive,
				ERasterizerState::NoCull,
				EDepthState::Disabled,
				EDrawingTopology::TriangleStrip,
				nullptr,
				_lightShapeVS,
				_lightSunPS,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr);

			{
				auto& vsSet = _lightShapeVS->GetLayoutSets();
				_lightShapeVSLayout = Make_GPU(SafeVkDescriptorSetLayout, owningDevice, vsSet.front().bindings);
			}
		}

		auto GetVS()
		{
			return _lightShapeVS;
		}

		auto GetSunPSO()
		{
			return _psoSunLight;
		}

		virtual void Shutdown(class GraphicsDevice* InOwner)
		{
			_lightShapeVS.Reset();
		}
	};

	GlobalDeferredLightingResources GVulkanDeferredLightingResrouces;

	PBRDeferredLighting::PBRDeferredLighting(VulkanRenderScene* InScene) : _owningScene(InScene)
	{
		_owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InScene->GetOwner());
		auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();
		auto sunPSO = GVulkanDeferredLightingResrouces.GetSunPSO();
		
		_viewOnlyVSSet = Make_GPU(SafeVkDescriptorSet, 
			_owningDevice, 
			sunPSO->GetDescriptorSetLayouts().front(), 
			globalSharedPool);

		auto cameraBuffer = InScene->GetCameraBuffer();		
		VkDescriptorBufferInfo perFrameInfo = cameraBuffer->GetDescriptorInfo();
		_viewOnlyVSSet->Update(
			{
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo }
			}
			);

		//
		auto deferredGbuffer = _owningDevice->GetColorTarget();
		auto& gbufferAttachments = deferredGbuffer->GetAttachments();

		_commonLightDescSet = Make_GPU(SafeVkDescriptorSet,
			_owningDevice,
			sunPSO->GetDescriptorSetLayouts().back(),
			globalSharedPool);

		VkSamplerCreateInfo createInfo = {};

		//fill the normal stuff
		createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		createInfo.magFilter = VK_FILTER_NEAREST;
		createInfo.minFilter = VK_FILTER_NEAREST;
		createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		createInfo.minLod = 0;
		createInfo.maxLod = 16.f;

		_nearestSampler = Make_GPU(SafeVkSampler,
			_owningDevice,
			createInfo);

		std::vector< VkDescriptorImageInfo > gbuffer;
		for (auto& curAttach : gbufferAttachments)
		{
			VkDescriptorImageInfo imageInfo;
			imageInfo.sampler = _nearestSampler->Get();
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageInfo.imageView = curAttach.view->Get();
			gbuffer.push_back(imageInfo);
		}
		
		SE_ASSERT(gbuffer.size() == 4);
		_viewOnlyVSSet->Update(
			{
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &gbuffer[0]},
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &gbuffer[1] },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &gbuffer[2] },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &gbuffer[3] }
			}
		);
	}

	// TODO cleanupppp
	//void PBRDeferredLighting::Render(RT_VulkanRenderableMesh& InVulkanRenderableMesh)
	//{
	//	auto currentFrame = _owningDevice->GetActiveFrame();
	//	auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

	//	auto vulkanMat = static_pointer_cast<RT_Vulkan_Material>(InVulkanRenderableMesh.GetMaterial());
	//	auto matCache = GetDeferredMaterialCache(VertexInputTypes::StaticMesh, vulkanMat);
	//	auto meshCache = GetMeshCache(InVulkanRenderableMesh);

	//	VkDeviceSize offsets[1] = { 0 };
	//	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshCache->vertexBuffer, offsets);
	//	vkCmdBindIndexBuffer(commandBuffer, meshCache->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	//	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
	//		matCache->state[(uint8_t)VertexInputTypes::StaticMesh]->GetVkPipeline());

	//	// if static we have everything pre cached
	//	if (InVulkanRenderableMesh.IsStatic())
	//	{
	//		uint32_t uniform_offsets[] = {
	//			(sizeof(GPUViewConstants)) * currentFrame,
	//			(sizeof(StaticDrawParams) * meshCache->staticLeaseIdx)
	//		};

	//		VkDescriptorSet locaDrawSets[] = {
	//			_camStaticBufferDescriptorSet->Get(),
	//			matCache->descriptorSet[0]->Get()
	//		};

	//		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
	//			matCache->state[(uint8_t)VertexInputTypes::StaticMesh]->GetVkPipelineLayout(),
	//			0,
	//			ARRAY_SIZE(locaDrawSets), locaDrawSets,
	//			ARRAY_SIZE(uniform_offsets), uniform_offsets);
	//	}
	//	// if not static we need to write transforms
	//	else
	//	{
	//		auto CurPool = _owningDevice->GetPerFrameResetDescriptorPool();
	//		auto vsLayout = GetOpaqueVSLayout();

	//		VkDescriptorSet dynamicTransformSet;
	//		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(CurPool, &vsLayout->Get(), 1);
	//		VK_CHECK_RESULT(vkAllocateDescriptorSets(_owningDevice->GetDevice(), &allocInfo, &dynamicTransformSet));

	//		auto cameraBuffer = _owningScene->GetCameraBuffer();

	//		VkDescriptorBufferInfo perFrameInfo;
	//		perFrameInfo.buffer = cameraBuffer->GetBuffer();
	//		perFrameInfo.offset = 0;
	//		perFrameInfo.range = cameraBuffer->GetPerElementSize();

	//		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
	//		vks::initializers::writeDescriptorSet(dynamicTransformSet,
	//			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
	//		vks::initializers::writeDescriptorSet(dynamicTransformSet,
	//			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &meshCache->transformBufferInfo),
	//		};

	//		vkUpdateDescriptorSets(_owningDevice->GetDevice(),
	//			static_cast<uint32_t>(writeDescriptorSets.size()),
	//			writeDescriptorSets.data(), 0, nullptr);

	//		uint32_t uniform_offsets[] = {
	//			(sizeof(GPUViewConstants)) * currentFrame,
	//			(sizeof(StaticDrawParams) * meshCache->staticLeaseIdx)
	//		};

	//		VkDescriptorSet locaDrawSets[] = {
	//			_camStaticBufferDescriptorSet->Get(),
	//			dynamicTransformSet
	//		};

	//		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
	//			matCache->state[(uint8_t)VertexInputTypes::StaticMesh]->GetVkPipelineLayout(),
	//			0,
	//			ARRAY_SIZE(locaDrawSets), locaDrawSets,
	//			ARRAY_SIZE(uniform_offsets), uniform_offsets);
	//	}

	//	vkCmdDrawIndexed(commandBuffer, meshCache->indexedCount, 1, 0, 0, 0);
	//}
}