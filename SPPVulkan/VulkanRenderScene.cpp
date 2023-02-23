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
#include "VulkanDepthDrawer.h"
#include "VulkanDeferredLighting.h"

#include "VulkanFrameBuffer.hpp"

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
	extern GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);

	static Vector3d HACKS_CameraPos;

	class GlobalVulkanRenderSceneResources : public GlobalGraphicsResource
	{
		GLOBAL_RESOURCE(GlobalVulkanRenderSceneResources)

	private:
		GPUReferencer < GPUShader > _fullscreenColorVS, _fullscreenColorPS;
		GPUReferencer< PipelineState > _fullscreenColorPSO;
		GPUReferencer< GPUInputLayout > _fullscreenColorLayout;

	public:
		// called on render thread
		GlobalVulkanRenderSceneResources() 
		{
			_fullscreenColorVS = Make_GPU(VulkanShader, EShaderType::Vertex);  
			_fullscreenColorVS->CompileShaderFromFile("shaders/fullScreenColorWrite.hlsl", "main_vs");

			_fullscreenColorPS = Make_GPU(VulkanShader, EShaderType::Pixel);
			_fullscreenColorPS->CompileShaderFromFile("shaders/fullScreenColorWrite.hlsl", "main_ps");

			_fullscreenColorLayout = Make_GPU(VulkanInputLayout);

			{
				auto& vulkanInputLayout = _fullscreenColorLayout->GetAs<VulkanInputLayout>();
				vulkanInputLayout.InitializeLayout(std::vector<VertexStream>());
			}

			auto backbufferFrameData = GGlobalVulkanGI->GetBackBufferFrameData();
			auto vulkPSO = Make_GPU(VulkanPipelineState);
			_fullscreenColorPSO = vulkPSO;
			vulkPSO->Initialize(backbufferFrameData,
				EBlendState::Disabled,
				ERasterizerState::NoCull,
				EDepthState::Disabled,
				EDrawingTopology::TriangleStrip,
				EDepthOp::Always,
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


	class GlobalOpaqueDrawerResources : public GlobalGraphicsResource
	{
		GLOBAL_RESOURCE(GlobalOpaqueDrawerResources)

	private:
		GPUReferencer < VulkanShader > _opaqueVS, _opaquePS, _opaquePSWithLightMap, _opaqueVSWithLightMap;
		GPUReferencer< SafeVkDescriptorSetLayout > _opaqueVSLayout;

		GPUReferencer< GPUInputLayout > _SMlayout;

	public:
		// called on render thread
		GlobalOpaqueDrawerResources() 
		{
			_opaqueVS = Make_GPU(VulkanShader, EShaderType::Vertex);
			_opaquePS = Make_GPU(VulkanShader, EShaderType::Pixel);
			_opaqueVSWithLightMap = Make_GPU(VulkanShader, EShaderType::Vertex);
			_opaquePSWithLightMap = Make_GPU(VulkanShader, EShaderType::Pixel);

			_opaqueVS->CompileShaderFromFile("shaders/SimpleTextureMesh.hlsl", "main_vs");
			_opaquePS->CompileShaderFromFile("shaders/SimpleTextureMesh.hlsl", "main_ps");
			_opaqueVSWithLightMap->CompileShaderFromFile("shaders/SimpleTextureLightMapMesh.hlsl", "main_vs");
			_opaquePSWithLightMap->CompileShaderFromFile("shaders/SimpleTextureLightMapMesh.hlsl", "main_ps");

			_SMlayout = Make_GPU(VulkanInputLayout);
			_SMlayout->InitializeLayout(OP_GetVertexStreams_SM());

			auto& vsSet = _opaqueVS->GetLayoutSets();
			_opaqueVSLayout = Make_GPU(SafeVkDescriptorSetLayout, vsSet.front().bindings);
			
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
	};
	
	REGISTER_GLOBAL_RESOURCE(GlobalVulkanRenderSceneResources);
	REGISTER_GLOBAL_RESOURCE(GlobalOpaqueDrawerResources);

	GPUReferencer< class SafeVkDescriptorSetLayout > GetOpaqueVSLayout()
	{
		return GGlobalVulkanGI->GetGlobalResource< GlobalOpaqueDrawerResources>()->GetOpaqueVSLayout();
	}
	
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
			auto owningDevice = GGlobalVulkanGI;

			cacheRef->state[(uint8_t)InVertexInputType] = InMat->GetPipelineState(EDrawingTopology::TriangleList,
				GGlobalVulkanGI->GetGlobalResource< GlobalOpaqueDrawerResources>()->GetOpaqueVS(),
				GGlobalVulkanGI->GetGlobalResource< GlobalOpaqueDrawerResources>()->GetOpaquePS(),
				GGlobalVulkanGI->GetGlobalResource< GlobalOpaqueDrawerResources>()->GetSMLayout());

			auto& descSetLayouts = cacheRef->state[(uint8_t)InVertexInputType]->GetDescriptorSetLayouts();

			const uint8_t TEXTURE_SET_ID = 1;
			VkDescriptorImageInfo textureInfo[4];

			auto globalSharedPool = owningDevice->GetPersistentDescriptorPool();
			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			int32_t TextureCount = 1;
			auto newTextureDescSet = Make_GPU(SafeVkDescriptorSet, descSetLayouts[TEXTURE_SET_ID]->Get(), globalSharedPool);

			auto& parameterMap = InMat->GetParameterMap();
			for (int32_t Iter = 0; Iter < TextureCount; Iter++)
			{
				auto getTexture = parameterMap.find("diffuse");
				GPUReferencer< GPUTexture > foundTexture;
				if (getTexture != parameterMap.end())
				{
					if (getTexture->second->GetType() == EMaterialParameterType::Texture)
					{
						auto thisRTTexture = std::dynamic_pointer_cast<RT_Texture>(getTexture->second);
						foundTexture = thisRTTexture->GetGPUTexture();
					}
				}

				auto gpuTexture = foundTexture ? foundTexture : owningDevice->GetDefaultTexture();
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


		
	class OpaqueDrawer
	{
	protected:
		GPUReferencer< SafeVkDescriptorSet > _camStaticBufferDescriptorSet;
		VulkanRenderScene* _owningScene = nullptr;

	public:
		OpaqueDrawer(VulkanRenderScene *InScene) : _owningScene(InScene)
		{
			auto _owningDevice = GGlobalVulkanGI;
			auto globalSharedPool = _owningDevice->GetPersistentDescriptorPool();

			auto meshVSLayout = GGlobalVulkanGI->GetGlobalResource< GlobalOpaqueDrawerResources>()->GetOpaqueVSLayout();
			_camStaticBufferDescriptorSet = Make_GPU(SafeVkDescriptorSet, meshVSLayout->Get(), globalSharedPool);

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
			auto _owningDevice = GGlobalVulkanGI;
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
				auto vsLayout = GGlobalVulkanGI->GetGlobalResource< GlobalOpaqueDrawerResources>()->GetOpaqueVSLayout();

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
	};

	
	

	VulkanRenderScene::VulkanRenderScene()
	{
		//SE_ASSERT(I());
		
		_debugDrawer = std::make_unique< VulkanDebugDrawing >();
	}

	VulkanRenderScene::~VulkanRenderScene()
	{
	}

	void VulkanRenderScene::AddedToGraphicsDevice()
	{
		auto owningDevice = GGlobalVulkanGI;
		auto globalSharedPool = owningDevice->GetPersistentDescriptorPool();

		_debugDrawer->Initialize();

		_fullscreenRayVS = Make_GPU(VulkanShader, EShaderType::Vertex);
		_fullscreenRayVS->CompileShaderFromFile("shaders/fullScreenRayVS.hlsl", "main_vs");

		_fullscreenRaySDFPS = Make_GPU(VulkanShader, EShaderType::Pixel);
		_fullscreenRaySDFPS->CompileShaderFromFile("shaders/fullScreenRaySDFPS.hlsl", "main_ps");

		_fullscreenRayVSLayout = Make_GPU(VulkanInputLayout); 

		{
			auto& vulkanInputLayout = _fullscreenRayVSLayout->GetAs<VulkanInputLayout>();
			vulkanInputLayout.InitializeLayout(std::vector<VertexStream>());
		}

		_fullscreenRaySDFPSO = VulkanPipelineStateBuilder()
			.Set(owningDevice->GetColorFrameData())
			.Set(EBlendState::Disabled)
			.Set(ERasterizerState::NoCull)
			.Set(EDepthState::Enabled)
			.Set(EDrawingTopology::TriangleStrip)
			.Set(EDepthOp::Always)
			.Set(_fullscreenRayVSLayout)
			.Set(_fullscreenRayVS)
			.Set(_fullscreenRaySDFPS)
			.Build();

		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		const auto InFlightFrames = GGlobalVulkanGI->GetInFlightFrames();

		_cameraData = std::make_shared< ArrayResource >();
		_cameraData->InitializeFromType< GPUViewConstants >(1);
		_cameraBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Simple, _cameraData);
		
		//static_assert(sizeof(GPURenderableCullData) % 4 == 0);

		// 512k loaded prims?
		_renderableCullData = std::make_shared< ArrayResource >();
		_renderableCullData->InitializeFromType< GPURenderableCullData >(512*1024);
		_renderableCullDataBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Simple, _renderableCullData);
		
		_renderableVisibleGPU = Make_GPU(VulkanBuffer, GPUBufferType::Simple, sizeof(uint32_t) * _renderableCullData->GetElementCount(), false);
		_renderableVisibleCPU = Make_GPU(VulkanBuffer, GPUBufferType::Simple, sizeof(uint32_t) * _renderableCullData->GetElementCount(), true);
		
		// common set
		_commonVS = Make_GPU(VulkanShader, EShaderType::Vertex);
		_commonVS->CompileShaderFromFile("shaders/CommonVS.glsl");

		// create common.glsl set
		{
			auto bindingsCopy = _commonVS->GetLayoutSets()[0].bindings;
			for (auto& curbinding : bindingsCopy)
			{
				curbinding.stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
			}
			_commonVSLayout = Make_GPU(SafeVkDescriptorSetLayout, bindingsCopy);

			_commonDescriptorSet = Make_GPU(SafeVkDescriptorSet,				
				_commonVSLayout->Get(),
				globalSharedPool);

			VkDescriptorBufferInfo camBufferInfo = { _cameraBuffer->GetBuffer(), 0, _cameraBuffer->GetPerElementSize() };
			_commonDescriptorSet->Update({
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &camBufferInfo},
				});
		}

		// create draw const set
		{
			auto staticDrawBuffer = GGlobalVulkanGI->GetStaticInstanceDrawBuffer();
			VkDescriptorBufferInfo drawConstsInfo = { staticDrawBuffer->GetBuffer(), 0, staticDrawBuffer->GetPerElementSize() };

			auto bindingsCopy = _commonVS->GetLayoutSets()[1].bindings;
			for (auto& curbinding : bindingsCopy)
			{
				curbinding.stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
			}
			auto drawConstLayout = Make_GPU(SafeVkDescriptorSetLayout, bindingsCopy);

			_drawConstDescriptorSet = Make_GPU(SafeVkDescriptorSet,
				drawConstLayout->Get(),
				globalSharedPool);

			_drawConstDescriptorSet->Update({
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &drawConstsInfo},
				});
		}


		// drawers
		_opaqueDrawer = std::make_unique< OpaqueDrawer >(this);
		_depthDrawer = std::make_unique< DepthDrawer >(this);
		_deferredDrawer = std::make_unique< PBRDeferredDrawer >(this);
		_deferredLightingDrawer = std::make_unique< PBRDeferredLighting >(this);
	}

	void VulkanRenderScene::RemovedFromGraphicsDevice()
	{
		_debugDrawer->Shutdown();

		_cameraBuffer.Reset();
		_cameraData.reset();
		_opaqueDrawer.reset();
		_depthDrawer.reset();
		_deferredDrawer.reset();
		_deferredLightingDrawer.reset();
	}

	void VulkanRenderScene::AddDebugLine(const Vector3d& Start, const Vector3d& End, const Vector3& Color)
	{
		_debugDrawer->AddDebugLine(Start, End, Color, true);
	}
	void VulkanRenderScene::AddDebugBox(const Vector3d& Center, const Vector3d& Extents, const Vector3& Color)
	{
		_debugDrawer->AddDebugBox(Center, Extents, Color, true);
	}
	void VulkanRenderScene::AddDebugSphere(const Vector3d& Center, float Radius, const Vector3& Color)
	{
		_debugDrawer->AddDebugSphere(Center, Radius, Color, true);
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
		return std::make_shared<VulkanRenderScene>();
	}

	// only called from graphics device, hence no need to heck
	void VulkanRenderScene::ResizeBuffers(int32_t NewWidth, int32_t NewHeight)
	{
		// resize it
		_depthDrawer.reset();
		_deferredLightingDrawer.reset();

		_depthDrawer = std::make_unique< DepthDrawer >(this);
		_deferredLightingDrawer = std::make_unique< PBRDeferredLighting >(this);
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
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		//UPDATE UNIFORMS
		_viewGPU.BuildCameraMatrices();
		_viewGPU.GetFrustumSpheresForRanges({ 15 }, _frustumRangeSpheres);
		_viewGPU.GetFrustumPlanes(_frustumPlanes);
		_cameraCullInfo = _viewGPU.GetCullingData();
		
		auto cameraSpan = _cameraData->GetSpan< GPUViewConstants>();
		GPUViewConstants& curCam = cameraSpan[0];
		curCam.ViewMatrix = _viewGPU.GetWorldToCameraMatrix();
		curCam.ViewProjectionMatrix = _viewGPU.GetViewProjMatrix();
		curCam.InvViewProjectionMatrix = _viewGPU.GetInvViewProjMatrix();
		curCam.InvProjectionMatrix = _viewGPU.GetInvProjectionMatrix();
		curCam.ViewPosition = _viewGPU.GetCameraPosition();
		curCam.FrameExtents = DeviceExtents;

		_cascadeSpheres.resize(_frustumRangeSpheres.size());
		for (size_t Iter = 0; Iter < _frustumRangeSpheres.size(); Iter++)
		{			
			Vector4 SphereLocation = ToVector4(_frustumRangeSpheres[Iter].GetCenter().cast<float>()) * _viewGPU.GetCameraMatrix();
			_cascadeSpheres[Iter] = Sphere(ToVector3(SphereLocation).cast<double>() + curCam.ViewPosition, _frustumRangeSpheres[Iter].GetRadius());
		}

		for (int32_t Iter = 0; Iter < _frustumPlanes.size(); Iter++)
		{
			curCam.FrustumPlanes[Iter] = _frustumPlanes[Iter].coeffs();
		}

		curCam.RecipTanHalfFovy = _viewGPU.GetRecipTanHalfFovy();
		_cameraBuffer->UpdateDirtyRegion(0, 1);
	}
		
	void VulkanRenderScene::OpaqueDepthPass()
	{
	}

	void VulkanRenderScene::OpaquePass()
	{
	}

	void VulkanRenderScene::Draw()
	{
		auto vulkanGD = GGlobalVulkanGI;

		auto currentFrame = vulkanGD->GetActiveFrame();
		auto DeviceExtents = vulkanGD->GetExtents();
		auto commandBuffer = vulkanGD->GetActiveCommandBuffer();
		auto vulkanDevice = vulkanGD->GetDevice();
		auto& scratchBuffer = vulkanGD->GetPerFrameScratchBuffer();

		auto &camPos = _viewGPU.GetCameraPosition();
		auto& camRot = _viewGPU.GetCameraRotation();
		std::string CameraLocText = std::string_format("%.1f %.1f %.1f", camPos[0], camPos[1], camPos[2]);
		std::string CameraRotText = std::string_format("%.1f %.1f %.1f", camRot[0], camRot[1], camRot[2]);
		vulkanGD->DrawDebugText(Vector2i(10, 20), CameraLocText.c_str() );
		vulkanGD->DrawDebugText(Vector2i(10, 40), CameraRotText.c_str());
		
		auto& depthOnlyFrame = vulkanGD->GetDepthOnlyFrameData();
		auto& defferedFrame = vulkanGD->GetDeferredFrameData();
		auto& lightingComposite = vulkanGD->GetLightingCompositeRenderPass();

		vulkanGD->SetFrameBufferForRenderPass(depthOnlyFrame);

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
		int32_t curVisibleLights = 0;

		if (_renderables.size() > _visible.size())
		{
			_visible.resize(_renderables.size() + 1024);
			_visiblelights.resize(_renderables.size() + 1024);
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

				if (curRenderable->GetType() == RenderableType::Mesh)
				{
					_visible[curVisible++] = curRenderable;
				}
				else if (curRenderable->GetType() == RenderableType::Light)
				{
					_visiblelights[curVisibleLights++] = curRenderable;
				}
								
				_octreeVisiblity.Set(curRenderable->GetGlobalID().RawGet()->GetID(), true);
				return true;
			},
		
			[&](const Vector3i& InCenter, int32_t InExtents) -> bool
			{
				double DistanceCalc = (double)InExtents / _frustumPlanes[4].absDistance(InCenter.cast<double>());
				return DistanceCalc > 0.02;
			}
		);
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

		vulkanGD->ConditionalEndRenderPass();

		vulkanGD->SetCheckpoint(commandBuffer, "DepthPyramid");

		_depthDrawer->ProcessDepthPyramid();

		vulkanGD->SetCheckpoint(commandBuffer, "DepthCulling");

		_depthDrawer->RunDepthCullingAgainstPyramid();

		vulkanGD->SetFrameBufferForRenderPass(defferedFrame);

		vulkanGD->SetCheckpoint(commandBuffer, "OpaqueDrawing");

#if 1

		uint32_t depthVisible = 0;
		for (uint32_t visIter = 0; visIter < curVisible; visIter++)
		{
			auto curID = _visible[visIter]->GetGlobalID().RawGet()->GetID();
			if (_depthCullVisiblity.Get(curID))
			{
				_deferredDrawer->Render(*(RT_VulkanRenderableMesh*)_visible[visIter]);
				depthVisible++;
			}
		}

		{
			std::string debugText = std::string_format("culling: %d %d", curVisible, depthVisible);
			vulkanGD->DrawDebugText(Vector2i(10, 30), debugText.c_str());
		}

#else
		for (auto& curVis : _renderables3d)
		{
			_opaqueDrawer->Render(*(RT_VulkanRenderableMesh*)curVis);
		}
#endif
				
		vulkanGD->ConditionalEndRenderPass();

		//Lighting
#if 1
		vulkanGD->SetFrameBufferForRenderPass(lightingComposite);
		vulkanGD->SetCheckpoint(commandBuffer, "Lighting");
		_deferredLightingDrawer->RenderSky();

		for (uint32_t visIter = 0; visIter < curVisibleLights; visIter++)
		{
			_deferredLightingDrawer->Render(*(RT_RenderableLight*)_visiblelights[visIter]);
		}

		{			
			vulkanGD->SetCheckpoint(commandBuffer, "DebugDrawing");
			_debugDrawer->Draw(this);
		}

		vulkanGD->ConditionalEndRenderPass();
#endif

#if 0
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
#endif

		WriteToFrame();
	};

	VkDescriptorSet VulkanRenderScene::GetCommondDescriptorSet()
	{
		return _overrideCommonDescriptorSet ? _overrideCommonDescriptorSet : _commonDescriptorSet->Get();
	}

	void VulkanRenderScene::WriteToFrame()
	{
		extern VkDevice GGlobalVulkanDevice;
		extern VulkanGraphicsDevice* GGlobalVulkanGI;

		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		auto backbufferFrameData = GGlobalVulkanGI->GetBackBufferFrameData();
		auto FullScreenWritePSO = GGlobalVulkanGI->GetGlobalResource< GlobalVulkanRenderSceneResources >()->GetFullScreenWritePSO();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = backbufferFrameData.renderPass->Get();
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = DeviceExtents[0];
		renderPassBeginInfo.renderArea.extent.height = DeviceExtents[1];
		renderPassBeginInfo.clearValueCount = 0;		
		renderPassBeginInfo.pClearValues = nullptr;
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = backbufferFrameData.frameBuffer->Get();

		// Start the first sub pass specified in our default render pass setup by the base class
		// This will clear the color and depth attachment
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = { 0, 0, float(DeviceExtents[0]), float(DeviceExtents[1]), 0, 1 };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// Update dynamic scissor state
		VkRect2D scissor = vks::initializers::rect2D(DeviceExtents[0], DeviceExtents[1], 0, 0);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		//
		auto CurPool = GGlobalVulkanGI->GetPerFrameResetDescriptorPool();
		auto& curPSO = FullScreenWritePSO->GetAs<VulkanPipelineState>();
		auto descriptorSetLayouts = curPSO.GetDescriptorSetLayoutsDirect();

		// main scene to back buffer
		{
			std::vector<VkDescriptorSet> locaDrawSets;
			locaDrawSets.resize(descriptorSetLayouts.size());

			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(CurPool, descriptorSetLayouts.data(), descriptorSetLayouts.size());
			VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice, &allocInfo, locaDrawSets.data()));

			VkDescriptorImageInfo textureInfo = GGlobalVulkanGI->GetLightCompositeFrameBuffer()->GetImageInfo();

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
#if 0
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
#endif

		vkCmdEndRenderPass(commandBuffer);
	}

	void VulkanRenderScene::DrawSkyBox()
	{
		//extern VkDevice GGlobalVulkanDevice;
		//extern VulkanGraphicsDevice* GGlobalVulkanGI;

		//auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
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