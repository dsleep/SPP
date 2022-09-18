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
		GPUReferencer < VulkanShader > _lightShapeVS;
		GPUReferencer < VulkanShader > _lightFullscreenVS;
		GPUReferencer< GPUInputLayout > _lightShapeLayout;

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

			_lightShapeLayout = Make_GPU(VulkanInputLayout, InOwner);
			_lightShapeLayout->InitializeLayout(OP_GetVertexStreams_DeferredLightingShapes());

			{
				auto& vsSet = _lightShapeVS->GetLayoutSets();
				_lightShapeVSLayout = Make_GPU(SafeVkDescriptorSetLayout, owningDevice, vsSet.front().bindings);
			}
		}

		auto GetVS()
		{
			return _lightShapeVS;
		}


		virtual void Shutdown(class GraphicsDevice* InOwner)
		{
			_lightShapeVS.Reset();
		}

	};

	GlobalDeferredLightingResources GVulkanDeferredLightingResrouces;

	PBRDeferredLighting::PBRDeferredLighting(VulkanRenderScene* InScene) : _owningScene(InScene)
	{
		/*_owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InScene->GetOwner());
		auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();

		auto meshVSLayout = GVulkanDeferredPBRResrouces.GetVSLayout();
		_camStaticBufferDescriptorSet = Make_GPU(SafeVkDescriptorSet, _owningDevice, meshVSLayout->Get(), globalSharedPool);

		auto cameraBuffer = InScene->GetCameraBuffer();

		VkDescriptorBufferInfo perFrameInfo;
		perFrameInfo.buffer = cameraBuffer->GetBuffer();
		perFrameInfo.offset = 0;
		perFrameInfo.range = cameraBuffer->GetPerElementSize();

		VkDescriptorBufferInfo drawConstsInfo;
		auto staticDrawBuffer = _owningDevice->GetStaticInstanceDrawBuffer();
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
			writeDescriptorSets.data(), 0, nullptr);*/
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