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

#include "SPPTextures.h"
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
		GLOBAL_RESOURCE(GlobalDeferredLightingResources)

	private:
		GPUReferencer < VulkanShader > _lightShapeVS, _lightFullscreenVS, _lightSunPS, _skyCubemapPS;
		GPUReferencer< GPUInputLayout > _lightShapeLayout;

		GPUReferencer< VulkanPipelineState > _psoSunLight, _psoSkyCube;
		GPUReferencer< SafeVkDescriptorSetLayout > _lightShapeVSLayout;
		GPUReferencer<SafeVkDescriptorSet> _skyCubePSDescSet;

		std::map< ParameterMapKey, GPUReferencer < VulkanShader > > _psShaderMap;

		GPUReferencer< VulkanTexture > _skyCube;

	public:
		// called on render thread
		GlobalDeferredLightingResources(class GraphicsDevice* InOwner) : GlobalGraphicsResource(InOwner)
		{
			auto owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InOwner);
			auto globalSharedPool = owningDevice->GetPersistentDescriptorPool();

			_lightShapeVS = Make_GPU(VulkanShader, InOwner, EShaderType::Vertex);
			_lightShapeVS->CompileShaderFromFile("shaders/Deferred/LightShapeVS.glsl");

			_lightFullscreenVS = Make_GPU(VulkanShader, InOwner, EShaderType::Vertex);
			_lightFullscreenVS->CompileShaderFromFile("shaders/Deferred/FullScreenLightVS.glsl");

			_lightSunPS = Make_GPU(VulkanShader, InOwner, EShaderType::Pixel);
			_lightSunPS->CompileShaderFromFile("shaders/Deferred/SunLightPS.glsl");

			_skyCubemapPS = Make_GPU(VulkanShader, InOwner, EShaderType::Pixel);
			_skyCubemapPS->CompileShaderFromFile("shaders/Deferred/SkyCubePS.glsl");

			_lightShapeLayout = Make_GPU(VulkanInputLayout, InOwner);
			_lightShapeLayout->InitializeLayout(OP_GetVertexStreams_DeferredLightingShapes());

			_psoSunLight = GetVulkanPipelineState(owningDevice,
				owningDevice->GetLightingCompositeRenderPass(),
				EBlendState::Disabled,
				ERasterizerState::NoCull,
				EDepthState::Disabled,
				EDrawingTopology::TriangleStrip,
				nullptr,
				_lightFullscreenVS,
				_lightSunPS,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr);

			_psoSkyCube = GetVulkanPipelineState(owningDevice,
				owningDevice->GetLightingCompositeRenderPass(),
				EBlendState::Disabled,
				ERasterizerState::NoCull,
				EDepthState::Disabled,
				EDrawingTopology::TriangleStrip,
				nullptr,
				_lightFullscreenVS,
				_skyCubemapPS,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr);

			{
				auto& vsSet = _lightShapeVS->GetLayoutSets();
				_lightShapeVSLayout = Make_GPU(SafeVkDescriptorSetLayout, owningDevice, vsSet.front().bindings);
			}


			//
			{
				TextureAsset loadSky;
				loadSky.LoadFromDisk(*AssetPath("/textures/SkyTextureOverCast_Cubemap.ktx2"));
				_skyCube = Make_GPU(VulkanTexture, owningDevice, loadSky);
			}

			_skyCubePSDescSet = Make_GPU(SafeVkDescriptorSet,
				owningDevice,
				_psoSkyCube->GetDescriptorSetLayouts()[2],
				globalSharedPool);

			auto skyDesc = _skyCube->GetDescriptor();
			_skyCubePSDescSet->Update(
				{
					{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &skyDesc },
				});
		}

		auto GetVS()
		{
			return _lightShapeVS;
		}

		auto GetSunPSO()
		{
			return _psoSunLight;
		}

		auto GetSkyPSO()
		{
			return _psoSkyCube;
		}
		auto GetSkyDescriptorSet()
		{
			return _skyCubePSDescSet;
		}

		auto GetSkyCube()
		{
			return _skyCube;
		}
	};

	REGISTER_GLOBAL_RESOURCE(GlobalDeferredLightingResources);

	PBRDeferredLighting::PBRDeferredLighting(VulkanRenderScene* InScene) : _owningScene(InScene)
	{
		_owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InScene->GetOwner());
		auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();
		auto sunPSO = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSunPSO();
		
		//
		auto deferredGbuffer = _owningDevice->GetColorTarget();
		auto& gbufferAttachments = deferredGbuffer->GetAttachments();

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

		// Update Gbuffers
		std::vector< VkDescriptorImageInfo > gbuffer;
		for (auto& curAttach : gbufferAttachments)
		{
			VkDescriptorImageInfo imageInfo;
			imageInfo.sampler = _nearestSampler->Get();
			imageInfo.imageLayout = curAttach.isDepthStencil() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL  : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = curAttach.view->Get();
			gbuffer.push_back(imageInfo);
		}
		
		_gbufferTextureSet = Make_GPU(SafeVkDescriptorSet,
			_owningDevice,
			sunPSO->GetDescriptorSetLayouts()[2],
			globalSharedPool);

		_dummySet = Make_GPU(SafeVkDescriptorSet,
			_owningDevice,
			sunPSO->GetDescriptorSetLayouts()[3],
			globalSharedPool);
		_dummySet->Update({});

		SE_ASSERT(gbuffer.size() == 4);
		_gbufferTextureSet->Update(
			{
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &gbuffer[0]},
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &gbuffer[1] },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &gbuffer[2] },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &gbuffer[3] }
			}
		);
	}

	struct alignas(16u) SunLightParams
	{
		Vector4 LightDirection;
		Vector4 Radiance;
	};

	void PBRDeferredLighting::RenderSky()
	{
		auto currentFrame = _owningDevice->GetActiveFrame();
		auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

		auto skyPSO = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSkyPSO();
		auto skyDesc = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSkyDescriptorSet();

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPSO->GetVkPipeline());

		// if static we have everything pre cached		
		uint32_t uniform_offsets[] = {
			(sizeof(GPUViewConstants)) * currentFrame
		};

		VkDescriptorSet locaDrawSets[] = {
			_owningScene->GetCommondDescriptorSet()->Get(),
			_dummySet->Get(),
			skyDesc->Get()
		};

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			skyPSO->GetVkPipelineLayout(),
			0,
			ARRAY_SIZE(locaDrawSets), locaDrawSets,
			ARRAY_SIZE(uniform_offsets), uniform_offsets);

		vkCmdDraw(commandBuffer, 4, 1, 0, 0);
	}

	// TODO cleanupppp
	void PBRDeferredLighting::Render(RT_RenderableLight& InLight)
	{
		auto currentFrame = _owningDevice->GetActiveFrame();
		auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

		auto sunPSO = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSunPSO();

		SunLightParams lightParams =
		{
			Vector4(0,-1,0,0),
			Vector4(1,1,1,0)
		};

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sunPSO->GetVkPipeline());
		vkCmdPushConstants(commandBuffer, sunPSO->GetVkPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SunLightParams), &lightParams);
		
		// if static we have everything pre cached		
		uint32_t uniform_offsets[] = {
			(sizeof(GPUViewConstants)) * currentFrame
		};

		VkDescriptorSet locaDrawSets[] = {
			_owningScene->GetCommondDescriptorSet()->Get(),
			_dummySet->Get(),
			_gbufferTextureSet->Get()
		};

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			sunPSO->GetVkPipelineLayout(),
			0,
			ARRAY_SIZE(locaDrawSets), locaDrawSets,
			ARRAY_SIZE(uniform_offsets), uniform_offsets);		

		vkCmdDraw(commandBuffer, 4, 1, 0, 0);
	}
}