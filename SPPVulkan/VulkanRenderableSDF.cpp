// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"

#include "VulkanDevice.h"
#include "VulkanRenderScene.h"
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
	extern GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	
	class VulkanSDF : public GD_RenderableSignedDistanceField
	{
	protected:		
		GPUReferencer< GPUBuffer > _shapeBuffer;
		std::shared_ptr< ArrayResource > _shapeResource;
		bool _bIsStatic = false;

		GPUReferencer< PipelineState > _customPSO;

		virtual void _AddToRenderScene(class GD_RenderScene* InScene) override;
	public:
		VulkanSDF(GraphicsDevice* InOwner) : GD_RenderableSignedDistanceField(InOwner) {}
		virtual void Draw() override;
		virtual void DrawDebug(std::vector< DebugVertex >& lines) override;
	};

	//std::shared_ptr<GD_RenderableSignedDistanceField> Vulkan_CreateSDF()
	//{
	//	return std::make_shared< VulkanSDF >();
	//}
		
	class V : public GPUResource
	{


	};

	void VulkanSDF::_AddToRenderScene(class GD_RenderScene* InScene)
	{
		GD_RenderableSignedDistanceField::_AddToRenderScene(InScene);

		_cachedRotationScale = Matrix4x4::Identity();
		_cachedRotationScale.block<3, 3>(0, 0) = GenerateRotationScale();

		static_assert((sizeof(SDFShape) * 8) % 128 == 0);

		if (!_shapes.empty())
		{
			_shapeResource = std::make_shared< ArrayResource >();
			auto pShapes = _shapeResource->InitializeFromType<SDFShape>(_shapes.size());
			memcpy(pShapes, _shapes.data(), _shapeResource->GetTotalSize());

			_shapeBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Array, _shapeResource);


			auto SDFVS = _parentScene->GetAs<VulkanRenderScene>().GetSDFVS();
			auto SDFLayout = _parentScene->GetAs<VulkanRenderScene>().GetRayVSLayout();

			if (_customShader)
			{
				_customPSO = GetVulkanPipelineState(EBlendState::Disabled,
					ERasterizerState::NoCull,
					EDepthState::Enabled,
					EDrawingTopology::TriangleList,
					SDFLayout,
					SDFVS,
					_customShader,
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					nullptr);
			}
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

		auto SDFVS = _parentScene->GetAs<VulkanRenderScene>().GetSDFVS();
		auto SDFPSO = _parentScene->GetAs<VulkanRenderScene>().GetSDFPSO();
		if (_customPSO)
		{
			SDFPSO = _customPSO;
		}

		//vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelines[compute.pipelineIndex]);
		//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelineLayout, 0, 1, &compute.descriptorSet, 0, 0);
		//vkCmdDispatch(commandBuffer, DeviceExtents[0] / 16, DeviceExtents[1] / 16, 1);
	}

	

}