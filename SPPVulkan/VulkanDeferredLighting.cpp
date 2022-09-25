// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"
#include "VulkanDevice.h"
#include "VulkanShaders.h"
#include "VulkanTexture.h"
#include "VulkanRenderableMesh.h"
#include "VulkanFrameBuffer.hpp"

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
		GPUReferencer<SafeVkDescriptorSet> _skyCubePSDescSet, _sunDescSet;

		std::map< ParameterMapKey, GPUReferencer < VulkanShader > > _psShaderMap;
				
		GPUReferencer< VulkanTexture > _skyCube, _specularBRDF_LUT, _textureIrradianceMap;

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
			//SKY CUBE
			{
				TextureAsset loadSky;
				loadSky.LoadFromDisk(*AssetPath("/textures/SkyTextureOverCast_Cubemap.ktx2"));
				_skyCube = Make_GPU(VulkanTexture, owningDevice, loadSky);
			}

			owningDevice->SubmitCopyCommands();
			vkDeviceWaitIdle(owningDevice->GetDevice());

			_skyCubePSDescSet = Make_GPU(SafeVkDescriptorSet,
				owningDevice,
				_psoSkyCube->GetDescriptorSetLayouts()[2],
				globalSharedPool);	

			auto skyDesc = _skyCube->GetDescriptor();
			_skyCubePSDescSet->Update(
				{
					{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &skyDesc },
				});

			// PBR LIGHTING 

			const int32_t BRDF_LUT_Size = 256;
			const uint32_t IrradianceMapSize = 32;

			_specularBRDF_LUT = Make_GPU(VulkanTexture, InOwner, BRDF_LUT_Size, BRDF_LUT_Size, 1, 1, TextureFormat::R16G16F, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
			_textureIrradianceMap = Make_GPU(VulkanTexture, InOwner, IrradianceMapSize, IrradianceMapSize, 1, 6, TextureFormat::R16G16B16A16F, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

			auto textureIrradianceDesc = _textureIrradianceMap->GetDescriptor();

			//GENERATE IRRADIANCE
			{
				// copy over
				auto vksDevice = owningDevice->GetVKSVulkanDevice();
				auto gQueue = owningDevice->GetGraphicsQueue(); //should be transfer
				VkCommandBuffer immediateCommand = vksDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

				auto _csComputeIRMap = Make_GPU(VulkanShader, InOwner, EShaderType::Compute);
				_csComputeIRMap->CompileShaderFromFile("shaders/PBRTools/irmap_cs.glsl");

				auto localPSO = GetVulkanPipelineState(owningDevice, _csComputeIRMap);

				auto csDescSet = Make_GPU(SafeVkDescriptorSet,
					owningDevice,
					localPSO->GetDescriptorSetLayouts()[0],
					globalSharedPool);

				csDescSet->Update(
					{
						{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &skyDesc },
						{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &textureIrradianceDesc }
					}
				);

				vkCmdBindPipeline(immediateCommand, VK_PIPELINE_BIND_POINT_COMPUTE, localPSO->GetVkPipeline());
				vkCmdBindDescriptorSets(immediateCommand,
					VK_PIPELINE_BIND_POINT_COMPUTE,
					localPSO->GetVkPipelineLayout(),
					0, 1, &csDescSet->Get(),
					0, nullptr);

				vkCmdDispatch(immediateCommand, IrradianceMapSize / 32, IrradianceMapSize / 32, 6);
				vksDevice->flushCommandBuffer(immediateCommand, gQueue);
			}

			//GENERATE BRDF SPEC LUT
			{
				// copy over
				auto vksDevice = owningDevice->GetVKSVulkanDevice();
				auto gQueue = owningDevice->GetGraphicsQueue(); //should be transfer
				VkCommandBuffer immediateCommand = vksDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

				auto csGenSpecularBRDF_LUT = Make_GPU(VulkanShader, InOwner, EShaderType::Compute);
				csGenSpecularBRDF_LUT->CompileShaderFromFile("shaders/PBRTools/spbrdf_cs.glsl");

				auto psoBRDFLut = GetVulkanPipelineState(owningDevice, csGenSpecularBRDF_LUT);

				auto lutDesc = _specularBRDF_LUT->GetDescriptor();

				auto csBRDF_LUTDescSet = Make_GPU(SafeVkDescriptorSet,
					owningDevice,
					psoBRDFLut->GetDescriptorSetLayouts()[0],
					globalSharedPool);

				csBRDF_LUTDescSet->Update(
					{
						{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &lutDesc }
					}
				);
			
				vkCmdBindPipeline(immediateCommand, VK_PIPELINE_BIND_POINT_COMPUTE, psoBRDFLut->GetVkPipeline());
				vkCmdBindDescriptorSets(immediateCommand,
					VK_PIPELINE_BIND_POINT_COMPUTE,
					psoBRDFLut->GetVkPipelineLayout(),
					0, 1, &csBRDF_LUTDescSet->Get(),
					0, nullptr);

				vkCmdDispatch(immediateCommand, BRDF_LUT_Size / 32, BRDF_LUT_Size / 32, 6);
				vksDevice->flushCommandBuffer(immediateCommand, gQueue);
			}



			auto _csFilterEnvMap = Make_GPU(VulkanShader, InOwner, EShaderType::Compute);
			_csFilterEnvMap->CompileShaderFromFile("shaders/PBRTools/spmap_cs.glsl");

			////;
			auto specLutDesc = _specularBRDF_LUT->GetDescriptor();

			_sunDescSet = Make_GPU(SafeVkDescriptorSet,
				owningDevice,
				_psoSunLight->GetDescriptorSetLayouts()[1],
				globalSharedPool);

			_sunDescSet->Update(
				{
					{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textureIrradianceDesc },
					{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &skyDesc },
					{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &specLutDesc },
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
		auto GetSunDescriptorSet()
		{
			return _sunDescSet;
		}
		auto GetSpecularBRDF_LUT()
		{
			return _specularBRDF_LUT;
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
			auto curTexture = curAttach.texture.get();

			VkDescriptorImageInfo imageInfo;
			imageInfo.sampler = _nearestSampler->Get();
			imageInfo.imageLayout = curTexture->isDepthStencil() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL  : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = curTexture->GetVkImageView();
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

		if (InLight.GetLightType() == ELightType::Sun)
		{
			auto sunPSO = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSunPSO();
			auto sunPRBSec = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSunDescriptorSet();

			Vector3 LightDir = -InLight.GetCachedRotationAndScale().block<1, 3>(1, 0);
			LightDir.normalize();
			SunLightParams lightParams =
			{
				ToVector4(LightDir),
				ToVector4(InLight.GetIrradiance())
			};

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sunPSO->GetVkPipeline());
			vkCmdPushConstants(commandBuffer, sunPSO->GetVkPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SunLightParams), &lightParams);

			// if static we have everything pre cached		
			uint32_t uniform_offsets[] = {
				(sizeof(GPUViewConstants)) * currentFrame
			};

			VkDescriptorSet locaDrawSets[] = {
				_owningScene->GetCommondDescriptorSet()->Get(),
				sunPRBSec->Get(),
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
}