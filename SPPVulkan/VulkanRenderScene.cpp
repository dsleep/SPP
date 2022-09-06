// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"
#include "VulkanDevice.h"
#include "VulkanShaders.h"
#include "VulkanTexture.h"
#include "VulkanRenderableMesh.h"

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

	enum class VertexInputTypes
	{
		StaticMesh = 0,
		SkeletalMesh,
		Particle,
		MAX
	};

	const std::vector<VertexStream>& OP_GetVertexStreams_SM()
	{
		static std::vector<VertexStream> vertexStreams;
		if (vertexStreams.empty())
		{
			MeshVertex dummy;
			vertexStreams.push_back(
				CreateVertexStream(dummy,
					dummy.position,
					dummy.normal,
					dummy.texcoord[0],
					dummy.texcoord[1],
					dummy.color));
		}
		return vertexStreams;
	}

	const std::vector<VertexStream>& Depth_GetVertexStreams_SM()
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

	class GlobalOpaqueDrawerResources : public GlobalGraphicsResource
	{
	private:
		GPUReferencer < VulkanShader > _opaqueVS, _opaquePS, _opaquePSWithLightMap, _opaqueVSWithLightMap;
		GPUReferencer< SafeVkDescriptorSetLayout > _opaqueVSLayout;

		GPUReferencer< GPUInputLayout > _SMlayout;

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

			_SMlayout = Make_GPU(VulkanInputLayout, InOwner);
			_SMlayout->InitializeLayout(OP_GetVertexStreams_SM());

			auto& vsSet = _opaqueVS->GetLayoutSets();
			_opaqueVSLayout = Make_GPU(SafeVkDescriptorSetLayout, owningDevice, vsSet.front().bindings);
		}

		GPUReferencer< SafeVkDescriptorSetLayout > GetOpaqueVSLayout()
		{
			return _opaqueVSLayout;
		}

		GPUReferencer< GPUInputLayout > GetSMLayout()
		{
			return _SMlayout;
		}

		GPUReferencer < VulkanShader > GetOpaqueVS()
		{
			return _opaqueVS;
		}

		GPUReferencer < VulkanShader > GetOpaquePS()
		{
			return _opaquePS;
		}

		virtual void Shutdown(class GraphicsDevice* InOwner)
		{
			_opaqueVS.Reset();
			_opaquePS.Reset();
			_opaqueVSWithLightMap.Reset();
			_opaquePSWithLightMap.Reset();
		}
	};

	

	class GlobalDepthDrawerResources : public GlobalGraphicsResource
	{
	private:
		GPUReferencer < VulkanShader > _depthVS;
		GPUReferencer< SafeVkDescriptorSetLayout > _depthVSLayout;

		GPUReferencer < VulkanShader > _depthPyramidCreationCS, _depthCullingCS;
		GPUReferencer< SafeVkDescriptorSetLayout > _depthPyramidCreationLayout, _depthCullingCSLayout;

		GPUReferencer< VulkanPipelineState > _depthPyramidPSO, _depthCullingPSO;

		GPUReferencer< SafeVkSampler > _depthPyramidSampler;

		GPUReferencer< GPUInputLayout > _SMDepthlayout;
		GPUReferencer< VulkanPipelineState > _SMDepthPSO;

	public:
		// called on render thread
		virtual void Initialize(class GraphicsDevice* InOwner)
		{
			auto owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InOwner);

			// DEPTH CULLING
			_depthCullingCS = Make_GPU(VulkanShader, InOwner, EShaderType::Compute);
			_depthCullingCS->CompileShaderFromFile("shaders/Depth/DepthPyramidCull.glsl", "main");

			{
				auto& layoutSet = _depthCullingCS->GetLayoutSets();
				_depthCullingCSLayout = Make_GPU(SafeVkDescriptorSetLayout, owningDevice, layoutSet.front().bindings);
			}

			_depthCullingPSO = GetVulkanPipelineState(InOwner,
				EBlendState::Disabled,
				ERasterizerState::NoCull,
				EDepthState::Enabled,
				EDrawingTopology::TriangleList,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				_depthCullingCS);

			// DEPTH PYRAMID
			_depthPyramidCreationCS = Make_GPU(VulkanShader, InOwner, EShaderType::Compute);
			_depthPyramidCreationCS->CompileShaderFromFile("shaders/Depth/DepthPyramidCompute.hlsl", "main_cs");

			_depthPyramidPSO = GetVulkanPipelineState(InOwner,
				EBlendState::Disabled,
				ERasterizerState::NoCull,
				EDepthState::Enabled,
				EDrawingTopology::TriangleList,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				_depthPyramidCreationCS);

			auto& depthPyrCreationCS = _depthPyramidCreationCS->GetLayoutSets();
			_depthPyramidCreationLayout = Make_GPU(SafeVkDescriptorSetLayout, owningDevice, depthPyrCreationCS.front().bindings);

			/// special min mod depth sampler
			VkSamplerCreateInfo createInfo = {};

			//fill the normal stuff
			createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			createInfo.magFilter = VK_FILTER_LINEAR;
			createInfo.minFilter = VK_FILTER_LINEAR;
			createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			createInfo.minLod = 0;
			createInfo.maxLod = 16.f;
			//createInfo.unnormalizedCoordinates = true;

			//add a extension struct to enable Min mode
			VkSamplerReductionModeCreateInfoEXT createInfoReduction = {};

			createInfoReduction.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT;
			createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;
			createInfo.pNext = &createInfoReduction;

			_depthPyramidSampler = Make_GPU(SafeVkSampler, owningDevice, createInfo);


			//DEPTH VS

			_depthVS = Make_GPU(VulkanShader, InOwner, EShaderType::Vertex);
			_depthVS->CompileShaderFromFile("shaders/Depth/DepthDrawVS.hlsl", "main_vs");

			_SMDepthlayout = Make_GPU(VulkanInputLayout, InOwner);
			_SMDepthlayout->InitializeLayout(Depth_GetVertexStreams_SM());

			_SMDepthPSO = GetVulkanPipelineState(InOwner,
				EBlendState::Disabled,
				ERasterizerState::BackFaceCull,
				EDepthState::Enabled,				
				EDrawingTopology::TriangleList,
				_SMDepthlayout,
				_depthVS,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr);			

			auto& vsSet = _depthVS->GetLayoutSets();
			_depthVSLayout = Make_GPU(SafeVkDescriptorSetLayout, owningDevice, vsSet.front().bindings);
		}

		GPUReferencer< SafeVkDescriptorSetLayout > GetVSLayout()
		{
			return _depthVSLayout;
		}

		auto GetDepthDrawingPSO()
		{
			return _SMDepthPSO;
		}

		auto GetDepthCSPyramidLayout()
		{
			return _depthPyramidCreationLayout;
		}

		auto GetDepthPyramidPSO()
		{
			return _depthPyramidPSO;
		}

		auto GetDepthCullingCS()
		{
			return _depthCullingCS;
		}
		auto GetDepthCullingPSO()
		{
			return _depthCullingPSO;
		}
		auto GepthCullingCSLayout()
		{
			return _depthCullingCSLayout;
		}


		GPUReferencer< GPUInputLayout > GetSMLayout()
		{
			return _SMDepthlayout;
		}

		GPUReferencer < VulkanShader > GetDepthVS()
		{
			return _depthVS;
		}

		GPUReferencer< SafeVkSampler > GetDepthSampler()
		{
			return _depthPyramidSampler;
		}

		virtual void Shutdown(class GraphicsDevice* InOwner)
		{
			_depthVS.Reset();
			_depthPyramidSampler.Reset();
		}
	};


	GlobalVulkanRenderSceneResources GVulkanSceneResrouces;
	GlobalOpaqueDrawerResources GVulkanOpaqueResrouces;
	GlobalDepthDrawerResources GVulkanDepthResrouces;


	struct OpaqueMaterialCache : PassCache
	{
		GPUReferencer< VulkanPipelineState > state[(uint8_t)VertexInputTypes::MAX];
		GPUReferencer< SafeVkDescriptorSet > descriptorSet[(uint8_t)VertexInputTypes::MAX];
		virtual ~OpaqueMaterialCache() {}
	};

	OpaqueMaterialCache* GetMaterialCache(VertexInputTypes InVertexInputType, std::shared_ptr<RT_Vulkan_Material> InMat)
	{
		const uint8_t OPAQUE_PBR_PASS = 0;
		auto& cached = InMat->GetPassCache()[OPAQUE_PBR_PASS];
		OpaqueMaterialCache* cacheRef = nullptr;
		if (!cached)
		{
			cached = std::make_unique< OpaqueMaterialCache >();
		}

		cacheRef = dynamic_cast<OpaqueMaterialCache*>(cached.get());

		if (!cacheRef->state[(uint8_t)InVertexInputType])
		{
			auto owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InMat->GetOwner());

			cacheRef->state[(uint8_t)InVertexInputType] = InMat->GetPipelineState(EDrawingTopology::TriangleList,
				GVulkanOpaqueResrouces.GetOpaqueVS(),
				GVulkanOpaqueResrouces.GetOpaquePS(),
				GVulkanOpaqueResrouces.GetSMLayout());

			auto& descSetLayouts = cacheRef->state[(uint8_t)InVertexInputType]->GetDescriptorSetLayouts();

			const uint8_t TEXTURE_SET_ID = 1;
			VkDescriptorImageInfo textureInfo[4];

			auto globalSharedPool = owningDevice->GetPersistentDescriptorPool();
			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			int32_t TextureCount = 1;
			auto newTextureDescSet = Make_GPU(SafeVkDescriptorSet, owningDevice, descSetLayouts[TEXTURE_SET_ID], globalSharedPool);

			auto& textureMap = InMat->GetTextureMap();
			for (int32_t Iter = 0; Iter < TextureCount; Iter++)
			{
				auto curTexturePurpose = (TexturePurpose)0;// textureBindings[Iter * 2].binding;
				auto getTexture = textureMap.find(curTexturePurpose);

				auto gpuTexture = getTexture != textureMap.end() ?
					getTexture->second->GetGPUTexture() :
					owningDevice->GetDefaultTexture();

				auto& currentVulkanTexture = gpuTexture->GetAs<VulkanTexture>();
				textureInfo[Iter] = currentVulkanTexture.GetDescriptor();

				writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(newTextureDescSet->Get(),
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, (Iter * 2) + 0, &textureInfo[Iter]));
				writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(newTextureDescSet->Get(),
					VK_DESCRIPTOR_TYPE_SAMPLER, (Iter * 2) + 1, &textureInfo[Iter]));
			}

			vkUpdateDescriptorSets(owningDevice->GetDevice(),
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);

			cacheRef->descriptorSet[(uint8_t)InVertexInputType] = newTextureDescSet;
		}

		return cacheRef;
	}

	struct OpaqueMeshCache : PassCache
	{
		VkBuffer indexBuffer = nullptr;
		VkBuffer vertexBuffer = nullptr;

		VkDescriptorBufferInfo transformBufferInfo;

		uint32_t staticLeaseIdx = 0;
		uint32_t indexedCount = 0;

		virtual ~OpaqueMeshCache() {}
	};

	OpaqueMeshCache* GetMeshCache(RT_VulkanRenderableMesh& InVulkanRenderableMesh)
	{
		const uint8_t OPAQUE_PBR_PASS = 0;
		auto& cached = InVulkanRenderableMesh.GetPassCache()[OPAQUE_PBR_PASS];

		if (!cached)
		{
			cached = std::make_unique< OpaqueMeshCache >();
		}

		auto cacheRef = dynamic_cast<OpaqueMeshCache*>(cached.get());

		if (!cacheRef->indexBuffer)
		{
			auto vulkSM = std::dynamic_pointer_cast<RT_VulkanStaticMesh>(InVulkanRenderableMesh.GetStaticMesh());

			auto gpuVertexBuffer = vulkSM->GetVertexBuffer()->GetGPUBuffer();
			auto gpuIndexBuffer = vulkSM->GetIndexBuffer()->GetGPUBuffer();

			auto& vulkVB = gpuVertexBuffer->GetAs<VulkanBuffer>();
			auto& vulkIB = gpuIndexBuffer->GetAs<VulkanBuffer>();

			cacheRef->indexBuffer = vulkIB.GetBuffer();
			cacheRef->vertexBuffer = vulkVB.GetBuffer();
			cacheRef->indexedCount = vulkIB.GetElementCount();

			if (InVulkanRenderableMesh.IsStatic())
			{
				cacheRef->staticLeaseIdx = InVulkanRenderableMesh.GetStaticDrawBufferIndex();
			}
			else
			{
				auto transformBuf = InVulkanRenderableMesh.GetDrawTransformBuffer();

				cacheRef->transformBufferInfo.buffer = transformBuf->GetBuffer();
				cacheRef->transformBufferInfo.offset = 0;
				cacheRef->transformBufferInfo.range = transformBuf->GetPerElementSize();
			}
		}

		return cacheRef;
	}

	//DEPTH PYRAMID
	//Hierarchial depth buffer etc...

	//MAT PASS

	//COLOR MATS

	//TRANS

	//POST

	//

	struct alignas(16u) DepthReduceData
	{
		Vector2 srcSize;
		Vector2 dstSize;
		uint32_t mipLevel;
	};

	class DepthDrawer
	{
	protected:		
		GPUReferencer< SafeVkDescriptorSet > _camStaticBufferDescriptorSet;
		GPUReferencer< SafeVkDescriptorSet > _depthPyramidDescriptorSet, _depthCullingDescriptorSet;

		VulkanGraphicsDevice* _owningDevice = nullptr;
		VulkanRenderScene* _owningScene = nullptr;

		GPUReferencer< VulkanTexture > _depthPyramidTexture;
		uint8_t _depthPyramidMips = 1;
		Vector2i _depthPyramidExtents;
		std::vector< GPUReferencer< SafeVkImageView > > _depthPyramidViews;

		std::vector< GPUReferencer< SafeVkDescriptorSet > > _depthPyramidDescriptors;

	public:
		DepthDrawer(VulkanRenderScene* InScene) : _owningScene(InScene)
		{
			_owningDevice = dynamic_cast<VulkanGraphicsDevice*>(InScene->GetOwner());
			auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();
				
			//
			auto depthVSLayout = GVulkanDepthResrouces.GetVSLayout();
			_camStaticBufferDescriptorSet = Make_GPU(SafeVkDescriptorSet, _owningDevice, depthVSLayout->Get(), globalSharedPool);

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

			{
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
			

			///////////////////////////////////
			// SETUP Depth pyramid texture
			///////////////////////////////////

			_depthPyramidDescriptorSet = Make_GPU(SafeVkDescriptorSet, _owningDevice, GVulkanDepthResrouces.GetDepthCSPyramidLayout()->Get(), globalSharedPool);

			auto DeviceExtents = _owningDevice->GetExtents();

			_depthPyramidExtents = Vector2i( roundUpToPow2(DeviceExtents[0]) >> 1, roundUpToPow2(DeviceExtents[1]) >> 1 );
			_depthPyramidMips = std::max(powerOf2(_depthPyramidExtents[0]), powerOf2(_depthPyramidExtents[1]));

			_depthPyramidTexture = Make_GPU(VulkanTexture, _owningDevice, _depthPyramidExtents[0], _depthPyramidExtents[1], _depthPyramidMips, TextureFormat::R32F,
				VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			_depthPyramidTexture->SetName("_depthPyramidTexture");

			_depthPyramidViews = _depthPyramidTexture->GetMipChainViews();

			auto ColorTarget = _owningDevice->GetColorTarget();
			auto& colorAttachment = ColorTarget->GetFrontAttachment();
			auto& depthAttachment = ColorTarget->GetBackAttachment();

			_depthPyramidDescriptors.clear();

			// create those descriptors of the mip chain
			auto depthDownsizeSampler = GVulkanDepthResrouces.GetDepthSampler();
			for (int32_t Iter = 0; Iter < _depthPyramidViews.size(); Iter++)
			{
				VkDescriptorImageInfo sourceTarget;
				sourceTarget.sampler = depthDownsizeSampler->Get();

				VkDescriptorImageInfo destTarget;
				destTarget.sampler = depthDownsizeSampler->Get();
				destTarget.imageView = _depthPyramidViews[Iter]->Get();
				destTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;				

				//for te first iteration, we grab it from the depth image
				if (Iter == 0)
				{
					sourceTarget.imageView = depthAttachment.view->Get();
					sourceTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
				//afterwards, we copy from a depth mipmap into the next
				else
				{
					sourceTarget.imageView = _depthPyramidViews[Iter - 1]->Get();
					sourceTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				}

				auto descChainSet = Make_GPU(SafeVkDescriptorSet, 
					_owningDevice, 
					GVulkanDepthResrouces.GetDepthCSPyramidLayout()->Get(), 
					globalSharedPool);

				{
					std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
					vks::initializers::writeDescriptorSet(descChainSet->Get(),
						VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0, &sourceTarget),
					vks::initializers::writeDescriptorSet(descChainSet->Get(),
						VK_DESCRIPTOR_TYPE_SAMPLER, 1, &sourceTarget),
					vks::initializers::writeDescriptorSet(descChainSet->Get(),
						VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &destTarget),
					};

					vkUpdateDescriptorSets(_owningDevice->GetDevice(),
						static_cast<uint32_t>(writeDescriptorSets.size()),
						writeDescriptorSets.data(), 0, nullptr);
				}

				_depthPyramidDescriptors.push_back(descChainSet);
			}

			//DEPTH CULLING AGAINST PYRAMID
			_depthCullingDescriptorSet = Make_GPU(SafeVkDescriptorSet, _owningDevice, GVulkanDepthResrouces.GepthCullingCSLayout()->Get(), globalSharedPool);

			VkDescriptorBufferInfo drawSphereInfo = _owningScene->GetCullDataBuffer()->GetDescriptorInfo();
			VkDescriptorBufferInfo drawVisibilityInfo = _owningScene->GetVisibleGPUBuffer()->GetDescriptorInfo();

			VkDescriptorImageInfo depthPyramidInfo;
			depthPyramidInfo.sampler = depthDownsizeSampler->Get();
			depthPyramidInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			depthPyramidInfo.imageView = _depthPyramidTexture->GetVkImageView();

			{
				std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
					vks::initializers::writeDescriptorSet(_depthCullingDescriptorSet->Get(),
						VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
					vks::initializers::writeDescriptorSet(_depthCullingDescriptorSet->Get(),
						VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, &drawSphereInfo),
					vks::initializers::writeDescriptorSet(_depthCullingDescriptorSet->Get(),
						VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 2, &drawVisibilityInfo),
					vks::initializers::writeDescriptorSet(_depthCullingDescriptorSet->Get(),
						VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &depthPyramidInfo),
				};

				vkUpdateDescriptorSets(_owningDevice->GetDevice(),
					static_cast<uint32_t>(writeDescriptorSets.size()),
					writeDescriptorSets.data(), 0, nullptr);
			}
		}

		inline uint32_t getGroupCount(uint32_t InValue, uint32_t threadCount)
		{
			return (InValue + threadCount - 1) / threadCount;
		}

		void ProcessDepthPyramid()
		{
			auto DeviceExtents = GGlobalVulkanGI->GetExtents();

			auto depthDownsizeSampler = GVulkanDepthResrouces.GetDepthSampler();
			auto depthPyrPSO = GVulkanDepthResrouces.GetDepthPyramidPSO();

			auto currentFrame = _owningDevice->GetActiveFrame();
			auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

			auto ColorTarget = _owningDevice->GetColorTarget();
			auto& colorAttachment = ColorTarget->GetFrontAttachment();
			auto& depthAttachment = ColorTarget->GetBackAttachment();

			vks::tools::setImageLayout(commandBuffer, depthAttachment.image->Get(),
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

			DepthReduceData reduceData = 
			{ 
				Vector2(DeviceExtents[0], DeviceExtents[1]),
				Vector2(DeviceExtents[0], DeviceExtents[1])
			};

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, depthPyrPSO->GetVkPipeline());

			for (int32_t Iter = 0; Iter < _depthPyramidViews.size(); Iter++)
			{
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, depthPyrPSO->GetVkPipelineLayout(), 0, 1, &_depthPyramidDescriptors[Iter]->Get(), 0, nullptr);

				uint32_t levelWidth = std::max(1, _depthPyramidExtents[0] >> Iter);
				uint32_t levelHeight = std::max(1, _depthPyramidExtents[1] >> Iter);

				// put dst into src
				std::swap(reduceData.dstSize, reduceData.srcSize);
				// shift dst
				reduceData.dstSize = { Vector2(levelWidth, levelHeight) };
				reduceData.mipLevel = Iter;

				//execute downsample compute shader
				vkCmdPushConstants(commandBuffer, depthPyrPSO->GetVkPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(reduceData), &reduceData);
				auto xCount = getGroupCount(levelWidth, 32);
				auto yCount = getGroupCount(levelHeight, 32);
				vkCmdDispatch(commandBuffer, xCount, yCount, 1);

				vks::tools::insertImageMemoryBarrier(commandBuffer,
					_depthPyramidTexture->GetVkImage(),
					
					VK_ACCESS_SHADER_WRITE_BIT,
					VK_ACCESS_SHADER_READ_BIT,

					VK_IMAGE_LAYOUT_GENERAL,
					VK_IMAGE_LAYOUT_GENERAL,

					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

					VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)Iter, 1, 0, 1 });
			}

			vks::tools::setImageLayout(commandBuffer, depthAttachment.image->Get(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
				{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });
		}

		void RunDepthCullingAgainstPyramid()
		{
			auto DeviceExtents = _owningDevice->GetExtents();
			auto depthDownsizeSampler = GVulkanDepthResrouces.GetDepthSampler();
			
			auto depthCullingPSO = GVulkanDepthResrouces.GetDepthCullingPSO();

			auto currentFrame = _owningDevice->GetActiveFrame();
			auto commandBuffer = _owningDevice->GetActiveCommandBuffer();


			GPUDrawCullData cullData;
			reinterpret_cast<CameraCullInfo&>(cullData) = _owningScene->GetCameraCullData();

			cullData.pyramidWidth = _depthPyramidExtents[0];
			cullData.pyramidHeight = _depthPyramidExtents[1];

			cullData.drawCount = _owningScene->GetMaxRenderableIdx();

			cullData.cullingEnabled = 1;
			cullData.occlusionEnabled = 1;

			uint32_t uniform_offsets[] = {
					(sizeof(GPUViewConstants)) * currentFrame,
					0,
					0
			};
			
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, depthCullingPSO->GetVkPipeline());
			vkCmdBindDescriptorSets(commandBuffer, 
				VK_PIPELINE_BIND_POINT_COMPUTE, 
				depthCullingPSO->GetVkPipelineLayout(), 
				0, 1,
				&_depthCullingDescriptorSet->Get(), ARRAY_SIZE(uniform_offsets), uniform_offsets);
			//execute downsample compute shader
			vkCmdPushConstants(commandBuffer, 
				depthCullingPSO->GetVkPipelineLayout(), 
				VK_SHADER_STAGE_COMPUTE_BIT, 
				0, 
				sizeof(cullData), &cullData);

			auto xCount = getGroupCount(cullData.drawCount, 64);
			vkCmdDispatch(commandBuffer, xCount, 1, 1);

			// copy over
			auto vksDevice = _owningDevice->GetVKSVulkanDevice();
			auto gQueue = _owningDevice->GetGraphicsQueue(); //should be transfer
			VkCommandBuffer copyCmd = vksDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			_owningScene->GetVisibleGPUBuffer()->CopyTo(copyCmd, *_owningScene->GetVisibleCPUBuffer().get());

			vksDevice->flushCommandBuffer(copyCmd, gQueue);

			auto& depthCull = _owningScene->GetDepthCullVisiblity();

			uint32_t* MappedCPUMem = (uint32_t*)_owningScene->GetVisibleCPUBuffer()->GetMappedMemory();
			for (uint32_t Iter = 0; Iter < cullData.drawCount; Iter++)
			{
				uint32_t byteIndex = (Iter >> 5);
				uint32_t bitIndex = 1 << (Iter - (byteIndex << 5));

				if (MappedCPUMem[byteIndex] & bitIndex)
				{
					depthCull.Set(Iter, true);
				}
				else
				{
					depthCull.Set(Iter, false);
				}
			}
		}
		
		void Render(RT_VulkanRenderableMesh& InVulkanRenderableMesh)
		{
			auto currentFrame = _owningDevice->GetActiveFrame();
			auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

			auto vulkanMat = static_pointer_cast<RT_Vulkan_Material>(InVulkanRenderableMesh.GetMaterial());
			auto meshPSO = GVulkanDepthResrouces.GetDepthDrawingPSO();
			auto meshCache = GetMeshCache(InVulkanRenderableMesh);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshCache->vertexBuffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, meshCache->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPSO->GetVkPipeline());

			// if static we have everything pre cached
			if (InVulkanRenderableMesh.IsStatic())
			{
				uint32_t uniform_offsets[] = {
					(sizeof(GPUViewConstants)) * currentFrame,
					(sizeof(StaticDrawParams) * meshCache->staticLeaseIdx)
				};

				VkDescriptorSet locaDrawSets[] = {
					_camStaticBufferDescriptorSet->Get()
				};

				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					meshPSO->GetVkPipelineLayout(),
					0,
					ARRAY_SIZE(locaDrawSets), locaDrawSets,
					ARRAY_SIZE(uniform_offsets), uniform_offsets);
			}
			// if not static we need to write transforms
			else
			{
				auto CurPool = _owningDevice->GetPerFrameResetDescriptorPool();
				auto vsLayout = GVulkanOpaqueResrouces.GetOpaqueVSLayout();

				VkDescriptorSet dynamicTransformSet;
				VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(CurPool, &vsLayout->Get(), 1);
				VK_CHECK_RESULT(vkAllocateDescriptorSets(_owningDevice->GetDevice(), &allocInfo, &dynamicTransformSet));

				auto cameraBuffer = _owningScene->GetCameraBuffer();

				VkDescriptorBufferInfo perFrameInfo;
				perFrameInfo.buffer = cameraBuffer->GetBuffer();
				perFrameInfo.offset = 0;
				perFrameInfo.range = cameraBuffer->GetPerElementSize();

				std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(dynamicTransformSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
				vks::initializers::writeDescriptorSet(dynamicTransformSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &meshCache->transformBufferInfo),
				};

				vkUpdateDescriptorSets(_owningDevice->GetDevice(),
					static_cast<uint32_t>(writeDescriptorSets.size()),
					writeDescriptorSets.data(), 0, nullptr);

				uint32_t uniform_offsets[] = {
					(sizeof(GPUViewConstants)) * currentFrame,
					(sizeof(StaticDrawParams) * meshCache->staticLeaseIdx)
				};

				VkDescriptorSet locaDrawSets[] = {
					_camStaticBufferDescriptorSet->Get(),
					dynamicTransformSet
				};

				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					meshPSO->GetVkPipelineLayout(),
					0,
					ARRAY_SIZE(locaDrawSets), locaDrawSets,
					ARRAY_SIZE(uniform_offsets), uniform_offsets);
			}

			vkCmdDrawIndexed(commandBuffer, meshCache->indexedCount, 1, 0, 0, 0);
		}


	};
		
	class OpaqueDrawer
	{
	protected:
		GPUReferencer< SafeVkDescriptorSet > _camStaticBufferDescriptorSet;
		VulkanGraphicsDevice* _owningDevice = nullptr;
		VulkanRenderScene* _owningScene = nullptr;

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


		// TODO cleanupppp
		void Render(RT_VulkanRenderableMesh &InVulkanRenderableMesh)
		{
			auto currentFrame = _owningDevice->GetActiveFrame();
			auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

			auto vulkanMat = static_pointer_cast<RT_Vulkan_Material>(InVulkanRenderableMesh.GetMaterial());
			auto matCache = GetMaterialCache(VertexInputTypes::StaticMesh, vulkanMat);
			auto meshCache = GetMeshCache(InVulkanRenderableMesh);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshCache->vertexBuffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, meshCache->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				matCache->state[(uint8_t)VertexInputTypes::StaticMesh]->GetVkPipeline());

			// if static we have everything pre cached
			if (InVulkanRenderableMesh.IsStatic())
			{				
				uint32_t uniform_offsets[] = {
					(sizeof(GPUViewConstants)) * currentFrame,
					(sizeof(StaticDrawParams) * meshCache->staticLeaseIdx)
				};

				VkDescriptorSet locaDrawSets[] = {
					_camStaticBufferDescriptorSet->Get(),
					matCache->descriptorSet[0]->Get()
				};
								
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					matCache->state[(uint8_t)VertexInputTypes::StaticMesh]->GetVkPipelineLayout(),
					0,
					ARRAY_SIZE(locaDrawSets), locaDrawSets,
					ARRAY_SIZE(uniform_offsets), uniform_offsets);
			}
			// if not static we need to write transforms
			else
			{
				auto CurPool = _owningDevice->GetPerFrameResetDescriptorPool();
				auto vsLayout = GVulkanOpaqueResrouces.GetOpaqueVSLayout();

				VkDescriptorSet dynamicTransformSet;
				VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(CurPool, &vsLayout->Get(), 1);
				VK_CHECK_RESULT(vkAllocateDescriptorSets(_owningDevice->GetDevice(), &allocInfo, &dynamicTransformSet));

				auto cameraBuffer = _owningScene->GetCameraBuffer();

				VkDescriptorBufferInfo perFrameInfo;
				perFrameInfo.buffer = cameraBuffer->GetBuffer();
				perFrameInfo.offset = 0;
				perFrameInfo.range = cameraBuffer->GetPerElementSize();

				std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(dynamicTransformSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
				vks::initializers::writeDescriptorSet(dynamicTransformSet,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &meshCache->transformBufferInfo),
				};

				vkUpdateDescriptorSets(_owningDevice->GetDevice(),
					static_cast<uint32_t>(writeDescriptorSets.size()),
					writeDescriptorSets.data(), 0, nullptr);

				uint32_t uniform_offsets[] = {
					(sizeof(GPUViewConstants)) * currentFrame,
					(sizeof(StaticDrawParams) * meshCache->staticLeaseIdx)
				};

				VkDescriptorSet locaDrawSets[] = {
					_camStaticBufferDescriptorSet->Get(),
					dynamicTransformSet
				};

				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					matCache->state[(uint8_t)VertexInputTypes::StaticMesh]->GetVkPipelineLayout(),
					0,
					ARRAY_SIZE(locaDrawSets), locaDrawSets,
					ARRAY_SIZE(uniform_offsets), uniform_offsets);
			}

			vkCmdDrawIndexed(commandBuffer, meshCache->indexedCount, 1, 0, 0, 0);
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
		_debugDrawer->Initialize();


		_fullscreenRayVS = Make_GPU(VulkanShader, _owner, EShaderType::Vertex);
		_fullscreenRayVS->CompileShaderFromFile("shaders/fullScreenRayVS.hlsl", "main_vs");

		_fullscreenRaySDFPS = Make_GPU(VulkanShader, _owner, EShaderType::Pixel);
		_fullscreenRaySDFPS->CompileShaderFromFile("shaders/fullScreenRaySDFPS.hlsl", "main_ps");

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
		
		//static_assert(sizeof(GPURenderableCullData) % 4 == 0);

		// 512k loaded prims?
		_renderableCullData = std::make_shared< ArrayResource >();
		_renderableCullData->InitializeFromType< GPURenderableCullData >(512*1024);
		_renderableCullDataBuffer = Vulkan_CreateStaticBuffer(_owner, GPUBufferType::Simple, _renderableCullData);
		
		_renderableVisibleGPU = Make_GPU(VulkanBuffer, _owner, GPUBufferType::Simple, sizeof(uint32_t) * _renderableCullData->GetElementCount(), false);
		_renderableVisibleCPU = Make_GPU(VulkanBuffer, _owner, GPUBufferType::Simple, sizeof(uint32_t) * _renderableCullData->GetElementCount(), true);
		
		// drawers
		_opaqueDrawer = std::make_unique< OpaqueDrawer >(this);
		_depthDrawer = std::make_unique< DepthDrawer >(this);		
	}

	void VulkanRenderScene::RemovedFromGraphicsDevice()
	{
		_debugDrawer->Shutdown();

		_cameraBuffer.Reset();
		_cameraData.reset();
		_opaqueDrawer.reset();
		_depthDrawer.reset();
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

		SE_ASSERT(_renderableCullData);

		auto& curID = InRenderable->GetGlobalID();
		SE_ASSERT(curID);
		auto thisID = curID.RawGet()->GetID();
		auto& curSphere = InRenderable->GetSphereBounds();

		auto cullDataSpan = _renderableCullData->GetSpan<GPURenderableCullData>();
		cullDataSpan[thisID].center = curSphere.GetCenter();
		cullDataSpan[thisID].radius = curSphere.GetRadius();
		_renderableCullDataBuffer->UpdateDirtyRegion(thisID, 1);
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

		for (int32_t Iter = 0; Iter < _maxRenderableIdx; Iter++)
		{
			if (_renderables[Iter])
			{
				_renderables[Iter]->PrepareToDraw();
			}
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
		_cameraCullInfo = _viewGPU.GetCullingData();

		auto cameraSpan = _cameraData->GetSpan< GPUViewConstants>();
		GPUViewConstants& curCam = cameraSpan[currentFrame];
		curCam.ViewMatrix = _viewGPU.GetWorldToCameraMatrix();
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
		auto vulkanGD = dynamic_cast<VulkanGraphicsDevice*>(_owner);

		auto currentFrame = vulkanGD->GetActiveFrame();
		auto basicRenderPass = vulkanGD->GetBaseRenderPass();
		auto DeviceExtents = vulkanGD->GetExtents();
		auto commandBuffer = vulkanGD->GetActiveCommandBuffer();
		auto vulkanDevice = vulkanGD->GetDevice();
		auto& scratchBuffer = vulkanGD->GetPerFrameScratchBuffer();

		auto &camPos = _viewGPU.GetCameraPosition();
		std::string CameraText = std::string_format("CAMERA: %.1f %.1f %.1f", camPos[0], camPos[1], camPos[2]);
		vulkanGD->DrawDebugText(Vector2i(10, 20), CameraText.c_str() );
		
		auto ColorTargetFrameData = vulkanGD->GetColorFrameData();

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

		
		// RENDER PROCESS

		// DEPTH PREPASS

		// LIGHT CALCS

		// FORWARD+ 


		//if (_skyBox)
		{
			//DrawSkyBox();
		}

		int32_t curVisible = 0;
		if (_renderables.size() > _visible.size())
		{
			_visible.resize(_renderables.size() + 1024);
		}

		_octreeVisiblity.Expand(_renderables.size());
		_depthCullVisiblity.Expand(_renderables.size());
		
		_octreeVisiblity.Clear();
		_depthCullVisiblity.Clear();

		//_opaques.resize(_renderables3d.size());
		//_translucents.resize(_renderables3d.size());

		//#if 1
		_octree.WalkElements(_frustumPlanes, [&](const IOctreeElement* InElement) -> bool
			{
				auto curRenderable = ((Renderable*)InElement);
				if (curRenderable->Is3dRenderable())
				{
					_visible[curVisible++] = curRenderable;
				}

				_octreeVisiblity.Set(curRenderable->GetGlobalID().RawGet()->GetID(), true);

				//auto& drawInfo = curRenderable->GetDrawingInfo();

				//drawInfo.drawingType == DrawingType::Opaque
				//((Renderable*)InElement)->Draw();
				return true;
			});
		//#else
		//	for (auto renderItem : _renderables3d)
		//	{
		//		renderItem->Draw();
		//	}
		//#endif

		vulkanGD->SetCheckpoint(commandBuffer, "DepthDrawing");

		for (uint32_t visIter = 0; visIter < curVisible; visIter++)
		{
			_depthDrawer->Render(*(RT_VulkanRenderableMesh*)_visible[visIter]);
		}

		vkCmdEndRenderPass(commandBuffer);

		vulkanGD->SetCheckpoint(commandBuffer, "DepthPyramid");

		_depthDrawer->ProcessDepthPyramid();

		vulkanGD->SetCheckpoint(commandBuffer, "DepthCulling");

		_depthDrawer->RunDepthCullingAgainstPyramid();

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		vulkanGD->SetCheckpoint(commandBuffer, "OpaqueDrawing");

#if 1

		for (uint32_t visIter = 0; visIter < curVisible; visIter++)
		{
			auto curID = _visible[visIter]->GetGlobalID().RawGet()->GetID();
			if (_depthCullVisiblity.Get(curID))
			{
				_opaqueDrawer->Render(*(RT_VulkanRenderableMesh*)_visible[visIter]);
			}
		}
#else
		for (auto& curVis : _renderables3d)
		{
			_opaqueDrawer->Render(*(RT_VulkanRenderableMesh*)curVis);
		}
#endif

		vulkanGD->SetCheckpoint(commandBuffer, "DebugDrawing");

		_debugDrawer->Draw(this);

		vkCmdEndRenderPass(commandBuffer);

		vulkanGD->SetCheckpoint(commandBuffer, "DepthPrepForPost");

		auto &DepthColorTexture = vulkanGD->GetDepthColor()->GetAs<VulkanTexture>();

		auto ColorTarget = vulkanGD->GetColorTarget();
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

		//TODO FIXME
		//for (auto renderItem : _renderablesPost)
		//{
		//	renderItem->Draw();
		//}

		vks::tools::setImageLayout(commandBuffer, colorAttachment.image->Get(),
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

		GGlobalVulkanGI->SetCheckpoint(commandBuffer, "WriteToFrame");

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