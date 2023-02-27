// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"
#include "VulkanDevice.h"
#include "VulkanShaders.h"
#include "VulkanTexture.h"
#include "VulkanRenderableMesh.h"

#include "VulkanDeferredDrawer.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"
#include "SPPSparseVirtualizedVoxelOctree.h"
#include "VulkanRenderableSVVO.h"

namespace SPP
{
	extern LogEntry LOG_VULKAN;

	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	enum class ParamType
	{
		Texture,
		Uniform,
		Constant
	};

	enum class ParamReturn
	{
		float1,
		float2,
		float3,
		float4,
	};

	//std::string ParamCast(ParamReturn InType)
	//{
	//	switch(InType)
	//	{
	//	case ParamReturn::float1:
	//		return ".x";
	//		break;
	//	case ParamReturn::float2:
	//		return "(.x";
	//		break;
	//	case ParamReturn::float3:
	//		break;
	//	case ParamReturn::float4:
	//		break;
	//	}

	//	SE_ASSERT(false);
	//	return "";
	//}

	enum class PBRParam
	{
		Diffuse,
		Opacity,
		Normal,
		Specular,
		Metallic,
		Roughness,
		Emissive,
	};

	struct PBRParamBlock
	{
		/*
		<<UNIFORM_BLOCK>>
		//
		<<DIFFUSE_BLOCK>>
		<<OPACITY_BLOCK>>
		<<NORMAL_BLOCK>>
		<<SPECULAR_BLOCK>>
		<<METALLIC_BLOCK>>
		<<ROUGHNESS_BLOCK>>
		<<EMISSIVE_BLOCK>>
		*/

		PBRParam param;
		std::string name;
		EMaterialParameterType::ENUM type;
		std::string defaultValue;
	};

	static const std::vector<PBRParamBlock> PRBDataSet =
	{
		{ PBRParam::Diffuse, "diffuse", EMaterialParameterType::Float3, "vec3( 0, 1, 0 )"},
		{ PBRParam::Opacity, "opacity", EMaterialParameterType::Float, "1.0f" },
		{ PBRParam::Normal, "normal", EMaterialParameterType::Float3, "vec3(0.5f, 0.5f, 1)" },
		{ PBRParam::Specular, "specular", EMaterialParameterType::Float, "0.5f" },
		{ PBRParam::Metallic, "metallic", EMaterialParameterType::Float, "0" },
		{ PBRParam::Roughness, "roughness", EMaterialParameterType::Float, "0.5f" },
		{ PBRParam::Emissive, "emissive", EMaterialParameterType::Float, "0" }
	};

	const std::vector<VertexStream>& OP_GetVertexStreams_Deferred()
	{
		static std::vector<VertexStream> vertexStreams;
		if (vertexStreams.empty())
		{
			MeshVertex dummy;
			vertexStreams.push_back(
				CreateVertexStream(dummy,
					dummy.position,
					dummy.texcoord[0],
					dummy.normal,
					dummy.tangent));
		}
		return vertexStreams;
	}

	class GlobalDeferredPBRResources : public GlobalGraphicsResource
	{
		GLOBAL_RESOURCE(GlobalDeferredPBRResources)

	private:
		GPUReferencer < VulkanShader > _defferedPBRVS, _defferedPBRVoxelPS;
		GPUReferencer< GPUInputLayout > _deferredSMlayout;
		GPUReferencer< SafeVkDescriptorSetLayout > _deferredVSLayout;

		std::map< ParameterMapKey, GPUReferencer < VulkanShader > > _psShaderMap;

	public:
		// called on render thread
		GlobalDeferredPBRResources() 
		{
			auto owningDevice = dynamic_cast<VulkanGraphicsDevice*>(GGI()->GetGraphicsDevice());

			_defferedPBRVoxelPS = Make_GPU(VulkanShader, EShaderType::Pixel);
			_defferedPBRVoxelPS->CompileShaderFromFile("shaders/Voxel/VoxelRayMarchPS.glsl");

			_defferedPBRVS = Make_GPU(VulkanShader, EShaderType::Vertex);
			_defferedPBRVS->CompileShaderFromFile("shaders/Deferred/PBRMaterialVS.glsl");

			_deferredSMlayout = Make_GPU(VulkanInputLayout);
			_deferredSMlayout->InitializeLayout(OP_GetVertexStreams_Deferred());

			{
				auto& vsSet = _defferedPBRVS->GetLayoutSets();
				_deferredVSLayout = Make_GPU(SafeVkDescriptorSetLayout, vsSet.front().bindings);
			}
		}

		auto GetVoxelRayMarch()
		{
			return _defferedPBRVoxelPS;
		}

		auto GetVS()
		{
			return _defferedPBRVS;
		}

		auto GetSMLayout()
		{
			return _deferredSMlayout;
		}

		auto GetVSLayout()
		{
			return _deferredVSLayout;
		}

		virtual void Shutdown(class GraphicsDevice* InOwner)
		{
			_defferedPBRVS.Reset();
		}

		GPUReferencer < VulkanShader > GetPSFromParamMap( const ParameterMapKey &ParamKey )
		{
			auto getRef = _psShaderMap.find(ParamKey);
			if (getRef != _psShaderMap.end())
			{
				return getRef->second;
			}
			return nullptr;
		}

		void SetPSForParamMap(const ParameterMapKey& ParamKey, GPUReferencer < VulkanShader > &InShader)
		{
			_psShaderMap[ParamKey] = InShader;
		}
	};

	REGISTER_GLOBAL_RESOURCE(GlobalDeferredPBRResources);

	
	PBRDeferredDrawer::PBRDeferredDrawer(VulkanRenderScene* InScene) : _owningScene(InScene)
	{
		_owningDevice = GGlobalVulkanGI;
		auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();

		auto meshVSLayout = _owningDevice->GetGlobalResource<GlobalDeferredPBRResources>()->GetVSLayout();
		_camStaticBufferDescriptorSet = Make_GPU(SafeVkDescriptorSet, meshVSLayout->Get(), globalSharedPool);

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
			writeDescriptorSets.data(), 0, nullptr);


		//Voxel Rendering
		auto FullScreenVoxelPBRPS = _owningDevice->GetGlobalResource<GlobalDeferredPBRResources>()->GetVoxelRayMarch();
		auto FullScreenVS = InScene->GetFullScreenVS();
		auto FullScreenVSLayout = InScene->GetEmpyVSLayout();

		_voxelPBRPSO = VulkanPipelineStateBuilder()
			.Set(GGlobalVulkanGI->GetDeferredFrameData())
			.Set(EBlendState::Disabled)
			.Set(ERasterizerState::NoCull)
			.Set(EDepthState::Enabled)
			.Set(EDrawingTopology::TriangleStrip)
			.Set(EDepthOp::Always)
			.Set(FullScreenVSLayout)
			.Set(FullScreenVS)
			.Set(FullScreenVoxelPBRPS)
			.Build();
	}

	struct DeferredMaterialCache : PassCache
	{
		GPUReferencer < VulkanShader > _defferedPBRPS;
		GPUReferencer< class VulkanPipelineState > state[(uint8_t)VertexInputTypes::MAX];
		GPUReferencer< class SafeVkDescriptorSet > descriptorSet[(uint8_t)VertexInputTypes::MAX];
		virtual ~DeferredMaterialCache() {}
	};

	//TODO HASH PARAM MAP
	//std::map<std::string, GPUReferencer < VulkanShader > > GDeferredPSMap;
	DeferredMaterialCache* GetDeferredMaterialCache(VertexInputTypes InVertexInputType, std::shared_ptr<RT_Vulkan_Material> InMat)
	{
		const uint8_t OPAQUE_PBR_PASS = 0;
		const uint8_t DEFERRED_PASS = 1;

		auto& cached = InMat->GetPassCache()[DEFERRED_PASS];
		DeferredMaterialCache* cacheRef = nullptr;
		if (!cached)
		{
			cached = std::make_unique< DeferredMaterialCache >();
		}

		cacheRef = dynamic_cast<DeferredMaterialCache*>(cached.get());

		if (!cacheRef->state[(uint8_t)InVertexInputType])
		{
			auto owningDevice = GGlobalVulkanGI;
			auto& paraMap = InMat->GetParameterMap();
			auto thisParamKey = ParameterMapKey(paraMap);
			auto foundPS = owningDevice->GetGlobalResource<GlobalDeferredPBRResources>()->GetPSFromParamMap(thisParamKey);

			std::vector<  GPUReferencer< GPUTexture > > texturesUsed;
			if (foundPS)
			{
				cacheRef->_defferedPBRPS = foundPS;

				// we still need to grab those textures
				for (const auto& pbrParams : PRBDataSet)
				{
					auto foundParam = paraMap.find(pbrParams.name);
					if (foundParam != paraMap.end())
					{
						auto& mappedParam = foundParam->second;
						if (mappedParam->GetType() == EMaterialParameterType::Texture)
						{
							auto paramValue = std::dynamic_pointer_cast<RT_Texture> (mappedParam)->GetGPUTexture();
							texturesUsed.push_back(paramValue);
						}
					}
				}
			}
			else
			{
				std::string UniformBlock;

				//layout(binding = 1) uniform sampler2D inImage;
				std::map<std::string, std::string> ReplacementMap;
				for (const auto& pbrParams : PRBDataSet)
				{
					std::string UpperParamName = std::str_to_upper(pbrParams.name);
					std::string BlockName = std::string_format("<<%s_BLOCK>>", UpperParamName.c_str());

					auto foundParam = paraMap.find(pbrParams.name);
					if (foundParam != paraMap.end())
					{
						auto& mappedParam = foundParam->second;

						if (mappedParam->GetType() >= EMaterialParameterType::Float &&
							mappedParam->GetType() <= EMaterialParameterType::Float4)
						{
							std::string stringConstSet;
							auto curType = mappedParam->GetType();
							switch (curType)
							{
							case EMaterialParameterType::Float:
							{
								auto paramValue = std::dynamic_pointer_cast<FloatParamter> (mappedParam)->Value;
								stringConstSet = std::string_format("return vec4(%f,0,0,0)", paramValue);
							}
								break;
							case EMaterialParameterType::Float2:
							{
								auto paramValue = std::dynamic_pointer_cast<Float2Paramter> (mappedParam)->Value;
								stringConstSet = std::string_format("return vec4(%f,%f,0,0)", paramValue[0], paramValue[1]);
							}
								break;
							case EMaterialParameterType::Float3:
							{
								auto paramValue = std::dynamic_pointer_cast<Float3Paramter> (mappedParam)->Value;
								stringConstSet = std::string_format("return vec4(%f,%f,%f,0)", paramValue[0], paramValue[1], paramValue[2]);
							}
								break;
							case EMaterialParameterType::Float4:
							{
								auto paramValue = std::dynamic_pointer_cast<Float4Paramter> (mappedParam)->Value;
								stringConstSet = std::string_format("return vec4(%f,%f,%f,%f)", paramValue[0], paramValue[1], paramValue[2], paramValue[3]);
							}
								break;
							}

							switch (pbrParams.type)
							{
							case EMaterialParameterType::Float:
								stringConstSet += ".x;";
								break;
							case EMaterialParameterType::Float2:
								stringConstSet += ".xy;";
								break;
							case EMaterialParameterType::Float3:
								stringConstSet += ".xyz;";
								break;
							case EMaterialParameterType::Float4:
								stringConstSet += ".xyzw;";
								break;
							}

							ReplacementMap[BlockName] = stringConstSet;
						}						
						else if (mappedParam->GetType() == EMaterialParameterType::Texture)
						{
							//texture(inImage, (vec2(pos) + vec2(0.5)) / imageSize).x;
							auto paramValue = std::dynamic_pointer_cast<RT_Texture> (mappedParam)->GetGPUTexture();

							std::string SamplerName = std::string_format("sampler_%s", UpperParamName.c_str());
							std::string TextureSampleString = std::string_format("return texture(%s, inUV)", SamplerName.c_str());

							UniformBlock += std::string_format("layout(set = 1, binding = %d) uniform sampler2D %s;\r\n", texturesUsed.size(), SamplerName.c_str());
							texturesUsed.push_back(paramValue);

							switch (pbrParams.type)
							{
							case EMaterialParameterType::Float:
								TextureSampleString += ".x;";
								break;
							case EMaterialParameterType::Float2:
								TextureSampleString += ".xy;";
								break;
							case EMaterialParameterType::Float3:
								TextureSampleString += ".xyz;";
								break;
							case EMaterialParameterType::Float4:
								TextureSampleString += ".xyzw;";
								break;
							}

							ReplacementMap[BlockName] = TextureSampleString;

						}
					}
					else
					{
						ReplacementMap[BlockName] = std::string_format("return %s;", pbrParams.defaultValue.c_str());
					}
				}

				ReplacementMap["<<UNIFORM_BLOCK>>"] = UniformBlock;

				cacheRef->_defferedPBRPS = Make_GPU(VulkanShader, EShaderType::Pixel);
				bool bCompiled = cacheRef->_defferedPBRPS->CompileShaderFromTemplate("shaders/Deferred/PBRMaterialTemplatePS.glsl", ReplacementMap);

				SE_ASSERT(bCompiled);

				owningDevice->GetGlobalResource<GlobalDeferredPBRResources>()->SetPSForParamMap(thisParamKey, cacheRef->_defferedPBRPS);
			}
						
			cacheRef->state[(uint8_t)InVertexInputType] = InMat->GetPipelineState(EDrawingTopology::TriangleList,
				owningDevice->GetGlobalResource<GlobalDeferredPBRResources>()->GetVS(),
				cacheRef->_defferedPBRPS,
				owningDevice->GetGlobalResource<GlobalDeferredPBRResources>()->GetSMLayout());

			auto& descSetLayouts = cacheRef->state[(uint8_t)InVertexInputType]->GetDescriptorSetLayouts();

			const uint8_t TEXTURE_SET_ID = 1;
			VkDescriptorImageInfo textureInfo[4];

			auto globalSharedPool = owningDevice->GetPersistentDescriptorPool();
			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			auto newTextureDescSet = Make_GPU(SafeVkDescriptorSet, descSetLayouts[TEXTURE_SET_ID]->Get(), globalSharedPool);

			auto& parameterMap = InMat->GetParameterMap();
			for (int32_t Iter = 0; Iter < texturesUsed.size(); Iter++)
			{												
				auto& currentVulkanTexture = texturesUsed[Iter]->GetAs<VulkanTexture>();

				if (currentVulkanTexture.GetVkImage() != nullptr)
				{
					textureInfo[Iter] = currentVulkanTexture.GetDescriptor();
				}
				else
				{
					textureInfo[Iter] = owningDevice->GetDefaultTexture()->GetAs<VulkanTexture>().GetDescriptor();
				}
				
				writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(newTextureDescSet->Get(),
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Iter, &textureInfo[Iter]));
			}

			vkUpdateDescriptorSets(owningDevice->GetDevice(),
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);

			cacheRef->descriptorSet[(uint8_t)InVertexInputType] = newTextureDescSet;
		}

		return cacheRef;
	}

	struct SVVODrawCache : PassCache
	{
		GPUReferencer< class SafeVkDescriptorSet > descriptorSet;
		virtual ~SVVODrawCache() {}
	};

	void PBRDeferredDrawer::RenderVoxelData(class RT_VulkanRenderableSVVO& InVoxelData)
	{
		const uint8_t OPAQUE_PBR_PASS = 0;
		const uint8_t DEFERRED_PASS = 1;

		auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();
		auto currentFrame = _owningDevice->GetActiveFrame();
		auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

		auto& cached = InVoxelData.GetPassCache()[OPAQUE_PBR_PASS];

		SVVODrawCache* cacheRef = nullptr;
		if (!cached)
		{
			cached = std::make_unique< SVVODrawCache >();
		}

		cacheRef = dynamic_cast<SVVODrawCache*>(cached.get());
				
		if (!cacheRef->descriptorSet)
		{		
			cacheRef->descriptorSet = Make_GPU(SafeVkDescriptorSet,
				_voxelPBRPSO->GetDescriptorSetLayouts()[1]->Get(),
				globalSharedPool);

			auto voxelBaseInfoBuf = InVoxelData.GetVoxelBaseInfo();
			auto voxelLeveInfoBuf = InVoxelData.GetVoxelLevelInfo();
			auto& sparseBuffers = InVoxelData.GetBuffers();

			{
				std::vector<VkWriteDescriptorSet> writeDescriptorSets;

				auto voxBaseInfoDescriptorInfo = voxelBaseInfoBuf->GetDescriptorInfo();
				auto voxLevelInfoDescriptorInfo = voxelLeveInfoBuf->GetDescriptorInfo();

				writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(
					cacheRef->descriptorSet->Get(),
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
					0,
					&voxBaseInfoDescriptorInfo));

				writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(
					cacheRef->descriptorSet->Get(),
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
					1,
					&voxLevelInfoDescriptorInfo));

				VkDescriptorBufferInfo sparseBuffers[MAX_VOXEL_LEVELS];
				for (int32_t Iter = 0; Iter < MAX_VOXEL_LEVELS; Iter++)
				{
					auto& curBuffer = InVoxelData.GetBufferLevel(Iter);
					auto& sparseVkBuf = curBuffer->GetGPUBuffer()->GetAs<VulkanBuffer>();

					sparseBuffers[Iter] = sparseVkBuf.GetDescriptorInfo();
				}

				writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(
					cacheRef->descriptorSet->Get(),
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					2,
					sparseBuffers,
					MAX_VOXEL_LEVELS));

				vkUpdateDescriptorSets(_owningDevice->GetDevice(),
					static_cast<uint32_t>(writeDescriptorSets.size()),
					writeDescriptorSets.data(), 0, nullptr);
			}
		}
	}

	// TODO cleanupppp
	void PBRDeferredDrawer::Render(RT_VulkanRenderableMesh& InVulkanRenderableMesh)
	{
		auto currentFrame = _owningDevice->GetActiveFrame();
		auto commandBuffer = _owningDevice->GetActiveCommandBuffer();

		auto vulkanMat = static_pointer_cast<RT_Vulkan_Material>(InVulkanRenderableMesh.GetMaterial());
		auto matCache = GetDeferredMaterialCache(VertexInputTypes::StaticMesh, vulkanMat);
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
				0,
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
			auto vsLayout = GetOpaqueVSLayout();

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
				0,
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
}