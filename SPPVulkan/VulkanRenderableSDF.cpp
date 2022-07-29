// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"

#include "VulkanDevice.h"
#include "VulkanRenderScene.h"
#include "VulkanTexture.h"
#include "SPPSDFO.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"


namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;
	extern LogEntry LOG_VULKAN;

	// lazy externs
	extern GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GraphicsDevice* InOwner, GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	
	class VulkanSDF : public RT_RenderableSignedDistanceField
	{
	protected:		

		std::shared_ptr< ArrayResource > _shapeResource;
		GPUReferencer< class VulkanBuffer > _shapeBuffer;

		std::shared_ptr< ArrayResource > _drawParams;
		GPUReferencer< class VulkanBuffer > _drawParamsBuffer;

		std::shared_ptr< ArrayResource > _drawConstants;
		GPUReferencer< class VulkanBuffer > _drawConstantsBuffer;


		bool bPendingUpdate = false;

		bool _bIsStatic = false;

		GPUReferencer< PipelineState > _customPSO;

		virtual void _AddToRenderScene(class RT_RenderScene* InScene) override;
	public:
		VulkanSDF(GraphicsDevice* InOwner) : RT_RenderableSignedDistanceField(InOwner) {}
		virtual void Draw() override;
		virtual void DrawDebug(std::vector< DebugVertex >& lines) override;
	};

	class GlobalVulkanSDFResources : public GlobalGraphicsResource
	{
	private:
		GPUReferencer < VulkanPipelineState > _PSO;
		std::shared_ptr< class RT_Shader > _CS;

	public:
		// called on render thread
		virtual void Initialize(class GraphicsDevice* InOwner)
		{
			//std::dynamic_pointer_cast<RT_Vulkan_Material>
			_CS = InOwner->CreateShader();
			_CS->Initialize(EShaderType::Compute);
			_CS->CompileShaderFromFile("shaders/SignedDistanceFieldCompute.hlsl", "main_cs");

			_PSO = GetVulkanPipelineState(InOwner,
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
				_CS->GetGPURef() );
		}

		auto GetPSO()
		{
			return _PSO;
		}

		virtual void Shutdown(class GraphicsDevice* InOwner)
		{
			_PSO.Reset();
			_CS.reset();
		}
	};

	GlobalVulkanSDFResources GVulkanSDFResrouces;

	//std::shared_ptr<RT_RenderableSignedDistanceField> Vulkan_CreateSDF()
	//{
	//	return std::make_shared< VulkanSDF >();
	//}
	
	std::shared_ptr< class RT_RenderableSignedDistanceField > VulkanGraphicsDevice::CreateSignedDistanceField()
	{
		return std::make_shared<VulkanSDF>(this);
	}

	void VulkanSDF::_AddToRenderScene(class RT_RenderScene* InScene)
	{
		RT_RenderableSignedDistanceField::_AddToRenderScene(InScene);

		_cachedRotationScale = Matrix4x4::Identity();
		_cachedRotationScale.block<3, 3>(0, 0) = GenerateRotationScale();

		static_assert((sizeof(SDFShape) * 8) % 128 == 0);

		if (!_shapes.empty())
		{
			_shapeResource = std::make_shared< ArrayResource >();
			auto pShapes = _shapeResource->InitializeFromType<SDFShape>(_shapes.size());
			memcpy(pShapes, _shapes.data(), _shapeResource->GetTotalSize());
			_shapeBuffer = Vulkan_CreateStaticBuffer(_owner, GPUBufferType::Array, _shapeResource);

			_drawParams = std::make_shared< ArrayResource >();
			auto pDrawParams = _drawParams->InitializeFromType< GPUDrawParams >(1);
			pDrawParams[0].ShapeColor = Vector3(1, 0, 0);
			pDrawParams[0].ShapeCount = _shapes.size();
			_drawParamsBuffer = Vulkan_CreateStaticBuffer(_owner, GPUBufferType::Simple, _drawParams);

			_drawConstants = std::make_shared< ArrayResource >();
			auto uniformData = _drawConstants->InitializeFromType<GPUDrawConstants>(1);
			auto& curData = uniformData[0];
			curData.LocalToWorldScaleRotation = _cachedRotationScale;
			curData.Translation = _position;
			_drawConstantsBuffer = Vulkan_CreateStaticBuffer(_owner, GPUBufferType::Simple, _drawConstants);
			bPendingUpdate = false;

			//if (_customShader)
			//{
			//	auto SDFVS = _parentScene->GetAs<VulkanRenderScene>().GetSDFVS();
			//	auto SDFLayout = _parentScene->GetAs<VulkanRenderScene>().GetRayVSLayout();

			//	_customPSO = GetVulkanPipelineState(EBlendState::Disabled,
			//		ERasterizerState::NoCull,
			//		EDepthState::Enabled,
			//		EDrawingTopology::TriangleList,
			//		SDFLayout,
			//		SDFVS,
			//		_customShader,
			//		nullptr,
			//		nullptr,
			//		nullptr,
			//		nullptr,
			//		nullptr);
			//}
		}
	}

	void VulkanSDF::DrawDebug(std::vector< DebugVertex >& lines)
	{
		//for (auto& curShape : _shapes)
		//{
		//	auto CurPos = GetPosition();
		//	DrawSphere(Sphere(curShape.translation + CurPos.cast<float>(), curShape.params[0]), lines);
		//}
	}

	void VulkanSDF::Draw()
	{
		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		auto csPSO = GVulkanSDFResrouces.GetPSO();

		auto activePool = GGlobalVulkanGI->GetActiveDescriptorPool();
		auto& descriptorSetLayouts = csPSO->GetDescriptorSetLayouts();

		std::vector<VkDescriptorSet> locaDrawSets;
		locaDrawSets.resize(descriptorSetLayouts.size());

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(activePool, descriptorSetLayouts.data(), descriptorSetLayouts.size());
		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice, &allocInfo, locaDrawSets.data()));

		auto parentScene = (VulkanRenderScene*)_parentScene;
		auto cameraBuffer = parentScene->GetCameraBuffer();

		VkDescriptorBufferInfo perFrameInfo;
		perFrameInfo.buffer = cameraBuffer->GetBuffer();
		perFrameInfo.offset = 0;
		perFrameInfo.range = cameraBuffer->GetPerElementSize();

		VkDescriptorBufferInfo drawConstsInfo;
		drawConstsInfo.buffer = _drawConstantsBuffer->GetBuffer();
		drawConstsInfo.offset = 0;
		drawConstsInfo.range = _drawConstantsBuffer->GetPerElementSize();

		auto ColorTarget = GGlobalVulkanGI->GetColorTarget();
		auto& colorAttachment = ColorTarget->GetFrontAttachment();

		auto& DepthColorTexture = GGlobalVulkanGI->GetDepthColor()->GetAs<VulkanTexture>();
		
		VkDescriptorBufferInfo drawParamsInfo;
		drawParamsInfo.buffer = _drawParamsBuffer->GetBuffer();
		drawParamsInfo.offset = 0;
		drawParamsInfo.range = _drawParamsBuffer->GetPerElementSize();

		VkDescriptorBufferInfo drawShapesInfo;
		drawShapesInfo.buffer = _shapeBuffer->GetBuffer();
		drawShapesInfo.offset = 0;
		drawShapesInfo.range = _shapeBuffer->GetDataSize();

		VkDescriptorImageInfo frameColorInfo = GGlobalVulkanGI->GetColorImageDescImgInfo();
		frameColorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		VkDescriptorImageInfo frameDepthInfo = DepthColorTexture.GetDescriptor();
		frameDepthInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(locaDrawSets[0],
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
			vks::initializers::writeDescriptorSet(locaDrawSets[0],
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &drawConstsInfo),
			
			vks::initializers::writeDescriptorSet(locaDrawSets[0],
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2, &drawParamsInfo),
			vks::initializers::writeDescriptorSet(locaDrawSets[0],
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 3, &drawShapesInfo),

			vks::initializers::writeDescriptorSet(locaDrawSets[0],
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4, &frameColorInfo),
			vks::initializers::writeDescriptorSet(locaDrawSets[0],
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 5, &frameDepthInfo)
		};

		vkUpdateDescriptorSets(vulkanDevice,
			static_cast<uint32_t>(writeDescriptorSets.size()),
			writeDescriptorSets.data(), 0, nullptr);

		uint32_t uniform_offsets[] = {
			(sizeof(GPUViewConstants)) * currentFrame,
			0,
			0,
			0
		};


		//vks::tools::setImageLayout(commandBuffer, depthAttachment.image->Get(),
		//	VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		//	VK_IMAGE_LAYOUT_GENERAL,
		//	{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 },
		//	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		//	VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, csPSO->GetVkPipeline());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, 
			csPSO->GetVkPipelineLayout(), 0, 
			locaDrawSets.size(), locaDrawSets.data(), 
			ARRAY_SIZE(uniform_offsets), uniform_offsets);
		vkCmdDispatch(commandBuffer, DeviceExtents[0] / 32, DeviceExtents[1] / 32, 1);		


		//vks::tools::setImageLayout(commandBuffer, depthAttachment.image->Get(),
		//	VK_IMAGE_LAYOUT_GENERAL,
		//	VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		//	{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 },
		//	VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		//	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}

	

}