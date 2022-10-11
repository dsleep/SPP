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
#include "VulkanDepthDrawer.h"

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

			_psoSunLight = VulkanPipelineStateBuilder(owningDevice)
				.Set(owningDevice->GetLightingCompositeRenderPass())
				.Set(EBlendState::Disabled)
				.Set(ERasterizerState::NoCull)
				.Set(EDepthState::Disabled)
				.Set(EDrawingTopology::TriangleStrip)
				.Set(EDepthOp::Always)
				.Set(_lightFullscreenVS)
				.Set(_lightSunPS)
				.Build();

			_psoSkyCube = VulkanPipelineStateBuilder(owningDevice)
				.Set(owningDevice->GetLightingCompositeRenderPass())
				.Set(EBlendState::Disabled)
				.Set(ERasterizerState::NoCull)
				.Set(EDepthState::Disabled)
				.Set(EDrawingTopology::TriangleStrip)
				.Set(EDepthOp::Always)
				.Set(_lightFullscreenVS)
				.Set(_skyCubemapPS)
				.Build();

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
				_psoSkyCube->GetDescriptorSetLayouts()[2]->Get(),
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

				auto localPSO = VulkanPipelineStateBuilder(owningDevice)
					.Set(_csComputeIRMap).Build();

				auto csDescSet = Make_GPU(SafeVkDescriptorSet,
					owningDevice,
					localPSO->GetDescriptorSetLayouts()[0]->Get(),
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

				auto psoBRDFLut = VulkanPipelineStateBuilder(owningDevice).Set(csGenSpecularBRDF_LUT).Build();

				auto lutDesc = _specularBRDF_LUT->GetDescriptor();

				auto csBRDF_LUTDescSet = Make_GPU(SafeVkDescriptorSet,
					owningDevice,
					psoBRDFLut->GetDescriptorSetLayouts()[0]->Get(),
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
		}

		auto GetShapeVS()
		{
			return _lightShapeVS;
		}
		auto GetFullScreenVS()
		{
			return _lightFullscreenVS;
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
		auto GetSpecularBRDF_LUT()
		{
			return _specularBRDF_LUT;
		}
		auto GetSkyCube()
		{
			return _skyCube;
		}

		auto GetSkyCubeTextureDescriptor()
		{
			return _skyCube->GetDescriptor();
		}
		auto GetIrradianceMapTextureDescriptor()
		{
			return _textureIrradianceMap->GetDescriptor();
		}
	};

	REGISTER_GLOBAL_RESOURCE(GlobalDeferredLightingResources);

	PBRDeferredLighting::PBRDeferredLighting(VulkanRenderScene* InScene) : _owningScene(InScene)
	{	
		_owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InScene->GetOwner());
		auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();
		auto sunPSO = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSunPSO();
		auto specularBRDF_LUT = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSpecularBRDF_LUT();

		auto skyCubeTextureDescriptor = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSkyCubeTextureDescriptor();
		auto irradianceMapTextureDescriptor = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetIrradianceMapTextureDescriptor();



		auto DeviceExtents = _owningDevice->GetExtents();
		
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

		GPUReferencer< class VulkanTexture > depthTexture;

		// Update Gbuffers
		std::vector< VkDescriptorImageInfo > gbuffer;
		for (auto& curAttach : gbufferAttachments)
		{
			auto curTexture = curAttach.texture.get();

			if (curTexture->isDepthStencil())
			{
				depthTexture = curAttach.texture;
			}

			VkDescriptorImageInfo imageInfo;
			imageInfo.sampler = _nearestSampler->Get();
			imageInfo.imageLayout = curTexture->isDepthStencil() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL  : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = curTexture->GetVkImageView();
			gbuffer.push_back(imageInfo);
		}
		
		_gbufferTextureSet = Make_GPU(SafeVkDescriptorSet,
			_owningDevice,
			sunPSO->GetDescriptorSetLayouts()[2]->Get(),
			globalSharedPool);

		_dummySet = Make_GPU(SafeVkDescriptorSet,
			_owningDevice,
			sunPSO->GetDescriptorSetLayouts()[3]->Get(),
			globalSharedPool);
		_dummySet->Update({});

		// gbuffer 
		SE_ASSERT(gbuffer.size() == 4);

		_gbufferTextureSet->Update(
			{
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &gbuffer[0]},
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &gbuffer[1] },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &gbuffer[2] },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &gbuffer[3] }
			}
		);
				
		////////////////////////////////////////////
		//SHADOW DEPTH FROM LIGHT PERSPECTIVE
		////////////////////////////////////////////
		_shadowDepthTexture = Make_GPU(VulkanTexture, _owningDevice, 1024, 1024, 1, 1,
			TextureFormat::D32_S8, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);		
		_shadowDepthFrameBuffer = std::make_unique< VulkanFramebuffer >(_owningDevice, 1024, 1024);
		_shadowDepthFrameBuffer->addAttachment(
			{
				.texture = _shadowDepthTexture,
				.name = "Depth"
			}
		);
		_shadowRenderPass = _shadowDepthFrameBuffer->createCustomRenderPass({ "Depth" }, VK_ATTACHMENT_LOAD_OP_CLEAR);
		_shadowRenderPass.bUseInvertedZ = false;

		////////////////////////////////////////////
		//SHADOW ATTENUATION
		////////////////////////////////////////////
		_shadowAttenuationTexture = Make_GPU(VulkanTexture, _owningDevice, DeviceExtents[0], DeviceExtents[1], 1, 1,
			TextureFormat::R8, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
		auto shadowAttenuationDesc = _shadowAttenuationTexture->GetDescriptor();
		_shadowAttenuation = std::make_unique< VulkanFramebuffer >(_owningDevice, DeviceExtents[0], DeviceExtents[1]);
		_shadowAttenuation->addAttachment(
			{
				.texture = _shadowAttenuationTexture,
				.name = "Attenuation",
				.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			}
		);
		_shadowAttenuation->addAttachment(
			{
				.texture = depthTexture,
				.name = "Depth",
				.initialLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
			}
		);
		_shadowAttenuationRenderPass = _shadowAttenuation->createCustomRenderPass(
			{ 
				{ "Attenuation", VK_ATTACHMENT_LOAD_OP_LOAD },
				{ "Depth", VK_ATTACHMENT_LOAD_OP_LOAD }
			});

		//shadow filter
		_shadowFilterPS = Make_GPU(VulkanShader, _owningDevice, EShaderType::Pixel);
		_shadowFilterPS->CompileShaderFromFile("shaders/Shadow/ShadowFilter.glsl");

		auto lightFullscreenVS = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetFullScreenVS();
		_shadowFilterPSO = VulkanPipelineStateBuilder(_owningDevice)
			.Set(_shadowAttenuationRenderPass)
			.Set(EBlendState::Disabled)
			.Set(ERasterizerState::NoCull)
			.Set(EDepthState::Disabled)
			.Set(EDrawingTopology::TriangleStrip)
			.Set(EDepthOp::Always)
			.Set(lightFullscreenVS)
			.Set(_shadowFilterPS)
			.Set(VK_DYNAMIC_STATE_DEPTH_BOUNDS)
			.Set(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE)
			.Build();

		// move down since it changes with size;
		_shadowFilterDescriptorSet = Make_GPU(SafeVkDescriptorSet,
			_owningDevice,
			_shadowFilterPSO->GetDescriptorSetLayouts()[2]->Get(),
			globalSharedPool);
		auto shadowDepthDesc = _shadowDepthTexture->GetDescriptor();
		_shadowFilterDescriptorSet->Update(
			{
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &gbuffer[3] },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &shadowDepthDesc }
			}
		);

		auto specLutDesc = specularBRDF_LUT->GetDescriptor();
		_sunDescSet = sunPSO->CreateDescriptorSet(1, globalSharedPool);
		_sunDescSet->Update(
			{
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &irradianceMapTextureDescriptor },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &skyCubeTextureDescriptor },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &specLutDesc },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &shadowAttenuationDesc }
			});
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
			0
		};

		VkDescriptorSet locaDrawSets[] = {
			_owningScene->GetCommondDescriptorSet(),
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

	void PBRDeferredLighting::RenderShadow(RT_RenderableLight& InLight)
	{
		auto DeviceExtents = _owningDevice->GetExtents();
		auto commandBuffer = _owningDevice->GetActiveCommandBuffer();
		auto currentFrame = _owningDevice->GetActiveFrame();
		auto commonVSLayout = _owningScene->GetCommonShaderLayout();
		auto& perFrameScratchBuffer = _owningDevice->GetPerFrameScratchBuffer();

		auto& sceneOctree = _owningScene->GetOctree();
		auto scratchPool = _owningDevice->GetPerFrameResetDescriptorPool();

		auto& sceneCam = _owningScene->GetGPUCamera();

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(scratchPool, &commonVSLayout->Get(), 1);

		auto& scratchBuffer = _owningDevice->GetPerFrameScratchBuffer();
		
		if (InLight.GetLightType() == ELightType::Sun)
		{
			_owningDevice->SetCheckpoint(commandBuffer, "SunShadowCascades");

			auto& cascadeSpheres = _owningScene->GetCascadeSpheres();

			// RENDER DEPTHS FROM SHADOW
			Planed cameraNearPlane = _owningScene->GetNearCameraPlane();
			std::vector<Planed> cascadePlanes;
			auto depthDrawer = _owningScene->GetDepthDrawer();


			Planed facingUp(Vector3d(0, 1, 0), 0);

			static std::vector< Renderable*> RenderableArray;
			auto minSize = _owningScene->GetMaxRenderableIdx() + 1;
			RenderableArray.resize(minSize);

			for (auto& curSphere : cascadeSpheres)
			{				
				_owningDevice->SetCheckpoint(commandBuffer, "CASCADE");

				// draw depths from light perspective
				_owningDevice->SetFrameBufferForRenderPass(_shadowRenderPass);
				

				auto curRadius = curSphere.GetRadius();
				Vector3d cascadeSphereCenter = curSphere.GetCenter();

				_owningScene->AddDebugLine(cascadeSphereCenter, cascadeSphereCenter + Vector3d(0, 5, 0));
				
				Camera orthoCam;
				// near far here irrelevant
				orthoCam.Initialize(cascadeSphereCenter, InLight.GetRotation(), Vector2(curRadius, curRadius), Vector2(0, curRadius));
				orthoCam.GetFrustumPlanes(cascadePlanes);
				
				Sphere totalBounds;
				uint32_t eleCount = 0;
				sceneOctree.WalkElements(cascadePlanes, [&](const IOctreeElement* InElement) -> bool
					{
						auto curRenderable = ((Renderable*)InElement);

						if (curRenderable->GetType() == RenderableType::Mesh)
						{			
							totalBounds += curRenderable->GetSphereBounds();
							RenderableArray[eleCount++] = curRenderable;							
						}
						return true;
					},

					[&](const Vector3i& InCenter, int32_t InExtents) -> bool
					{
						double DistanceCalc = (double)InExtents / cameraNearPlane.absDistance(InCenter.cast<double>());
						return DistanceCalc > 0.02;
					}
					);

				Vector3 LightDir = InLight.GetCachedRotationAndScale().block<1, 3>(2, 0);

				Planed sphereEdgePlane(
					LightDir.cast<double>(),
					totalBounds.GetCenter() + -LightDir.cast<double>() * totalBounds.GetRadius());
				double DistanceToSphereEdge = sphereEdgePlane.signedDistance(cascadeSphereCenter);
				cascadeSphereCenter += -LightDir.cast<double>() * DistanceToSphereEdge;

				// reinit it
				orthoCam.Initialize(cascadeSphereCenter, InLight.GetRotation(), Vector2(curRadius, curRadius), Vector2(0, totalBounds.GetRadius() + curRadius));

				Vector3 OutFrustumCorners[8];
				orthoCam.GetFrustumCorners(OutFrustumCorners);

				for (int32_t Iter = 0; Iter < 4; Iter++)
				{
					_owningScene->AddDebugLine(cascadeSphereCenter + OutFrustumCorners[Iter].cast < double>(),
						cascadeSphereCenter + OutFrustumCorners[Iter + 4].cast < double>());

					int32_t nextLine = (Iter + 1) % 4;
					_owningScene->AddDebugLine(cascadeSphereCenter + OutFrustumCorners[Iter].cast < double>(),
						cascadeSphereCenter + OutFrustumCorners[nextLine].cast < double>());
					_owningScene->AddDebugLine(cascadeSphereCenter + OutFrustumCorners[Iter + 4].cast < double>(),
						cascadeSphereCenter + OutFrustumCorners[nextLine + 4].cast < double>());
				}

				GPUViewConstants cameraData;
				cameraData.ViewMatrix = orthoCam.GetWorldToCameraMatrix();
				cameraData.ViewProjectionMatrix = orthoCam.GetViewProjMatrix();
				cameraData.InvViewProjectionMatrix = orthoCam.GetInvViewProjMatrix();
				cameraData.InvProjectionMatrix = orthoCam.GetInvProjectionMatrix();
				cameraData.ViewPosition = orthoCam.GetCameraPosition();
				cameraData.FrameExtents = Vector2i(1024, 1024);

				auto bufferSlice = scratchBuffer.Write((uint8_t*)&cameraData, sizeof(cameraData), currentFrame);

				VkDescriptorSet commonSetOverride;
				VK_CHECK_RESULT(vkAllocateDescriptorSets(_owningDevice->GetDevice(), &allocInfo, &commonSetOverride));

				VkDescriptorBufferInfo lightCommon;
				lightCommon.buffer = bufferSlice.buffer;
				lightCommon.offset = bufferSlice.offsetFromBase;
				lightCommon.range = bufferSlice.size;
				std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
					vks::initializers::writeDescriptorSet(commonSetOverride, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &lightCommon)
				};
				vkUpdateDescriptorSets(_owningDevice->GetDevice(), static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
				_owningScene->SetCommonDescriptorOverride(commonSetOverride);

				for (uint32_t Iter = 0; Iter < eleCount; Iter++)
				{
					depthDrawer->Render(*(RT_VulkanRenderableMesh*)RenderableArray[Iter], false);
				}

				_owningScene->SetCommonDescriptorOverride(nullptr);
				_owningDevice->ConditionalEndRenderPass();

				//shadow filtering pass
				{
					
					VkClearColorValue ClearColorValue = { 1.0, 0.0, 0.0, 0.0 };


					vks::tools::setImageLayout(
						commandBuffer,
						_shadowAttenuationTexture->GetVkImage(),
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_IMAGE_LAYOUT_GENERAL,
						_shadowAttenuationTexture->GetSubresourceRange());

					vkCmdClearColorImage(commandBuffer,
						_shadowAttenuationTexture->GetVkImage(),
						VK_IMAGE_LAYOUT_GENERAL,
						&ClearColorValue, 
						1, 
						&_shadowAttenuationTexture->GetSubresourceRange());

					vks::tools::setImageLayout(
						commandBuffer,
						_shadowAttenuationTexture->GetVkImage(),
						VK_IMAGE_LAYOUT_GENERAL,
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						_shadowAttenuationTexture->GetSubresourceRange());

					////vkCmdClearColorImage clear on first pass
					//
					// RENDER TO SHADOW ATTENUATION
					_owningDevice->SetFrameBufferForRenderPass(_shadowAttenuationRenderPass);
					_owningDevice->SetCheckpoint(commandBuffer, "ShadowAttenuation");
					
					vkCmdSetDepthBounds(commandBuffer, 0, 1);
					vkCmdSetDepthBoundsTestEnable(commandBuffer, VK_TRUE);

					Vector3d camPosition = sceneCam.GetCameraPosition() - orthoCam.GetCameraPosition();

					Matrix4x4 translationMat = Matrix4x4::Identity();
					translationMat.block<1, 3>(3, 0) = camPosition.cast<float>();

					Matrix4x4 NDCToTex = Matrix4x4{
						{ 0.5f, 0,		0,		0 },
						{ 0,	-0.5f,	0,		0 },
						{ 0,	0,		1.0f,	0 },
						{ 0.5f, 0.5f,	0,		1.0f}
					};

					struct alignas(16u) ShadowParams
					{
						Matrix4x4 sceneToShadow;
					};

					ShadowParams params;
					params.sceneToShadow = sceneCam.GetInvViewProjMatrix() * translationMat * orthoCam.GetViewProjMatrix() * NDCToTex;

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowFilterPSO->GetVkPipeline());
					vkCmdPushConstants(commandBuffer, _shadowFilterPSO->GetVkPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShadowParams), &params);

					// if static we have everything pre cached		
					uint32_t uniform_offsets[] = {
						0
					};

					VkDescriptorSet locaDrawSets[] = {
						_owningScene->GetCommondDescriptorSet(),
						_dummySet->Get(),
						_shadowFilterDescriptorSet->Get()
					};

					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
						_shadowFilterPSO->GetVkPipelineLayout(),
						0,
						ARRAY_SIZE(locaDrawSets), locaDrawSets,
						ARRAY_SIZE(uniform_offsets), uniform_offsets);

					vkCmdDraw(commandBuffer, 4, 1, 0, 0);

					_owningDevice->ConditionalEndRenderPass();

				}
			}
		}
	}

	// TODO cleanupppp
	void PBRDeferredLighting::Render(RT_RenderableLight& InLight)
	{
		auto currentFrame = _owningDevice->GetActiveFrame();
		auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

		if (InLight.GetLightType() == ELightType::Sun)
		{
			auto& lightingComposite = _owningDevice->GetLightingCompositeRenderPass();

			RenderShadow(InLight);
			_owningDevice->SetFrameBufferForRenderPass(lightingComposite);
			_owningDevice->SetCheckpoint(commandBuffer, "Sun");

			auto sunPSO = _owningDevice->GetGlobalResource< GlobalDeferredLightingResources >()->GetSunPSO();

			Vector3 LightDir = InLight.GetCachedRotationAndScale().block<1, 3>(2, 0);
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
				0
			};

			VkDescriptorSet locaDrawSets[] = {
				_owningScene->GetCommondDescriptorSet(),
				_sunDescSet->Get(),
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