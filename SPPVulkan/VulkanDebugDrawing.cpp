// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "VulkanDebugDrawing.h"
#include "VulkanDevice.h"
#include "VulkanRenderScene.h"


namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	extern GPUReferencer< GPUInputLayout > Vulkan_CreateInputLayout();;

	extern GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		EDrawingTopology InTopology,
		GPUReferencer< GPUInputLayout > InLayout,
		GPUReferencer< GPUShader> InVS,
		GPUReferencer< GPUShader> InPS,
		GPUReferencer< GPUShader> InMS,
		GPUReferencer< GPUShader> InAS,
		GPUReferencer< GPUShader> InHS,
		GPUReferencer< GPUShader> InDS,
		GPUReferencer< GPUShader> InCS);

	struct ColoredVertex
	{
		Vector3 position;
		Vector3 color;
	};

	const std::vector<VertexStream>& GetVertexStreams(const ColoredVertex& InPlaceholder)
	{
		static std::vector<VertexStream> vertexStreams;
		if (vertexStreams.empty())
		{
			vertexStreams.push_back(CreateVertexStream(InPlaceholder, InPlaceholder.position, InPlaceholder.color));
		}
		return vertexStreams;
	}

	struct VulkanDebugDrawing::DataImpl
	{
	private:
		GPUReferencer < VulkanPipelineState > _PSO;
		std::shared_ptr< class GD_Shader > _simpleVS;
		std::shared_ptr< class GD_Shader > _simplePS;
		std::shared_ptr<GD_Material> _simpleDebugMaterial;
		GPUReferencer< GPUInputLayout > _layout;
		GPUReferencer < VulkanPipelineState > _state;

	public:
		// called on render thread
		virtual void Initialize(class GraphicsDevice* InOwner)
		{
			_simpleDebugMaterial = InOwner->CreateMaterial();

			_simpleVS = InOwner->CreateShader();
			_simplePS = InOwner->CreateShader();

			_simpleDebugMaterial->SetMaterialArgs({ .vertexShader = _simpleVS, .pixelShader = _simplePS });

			_simpleVS->Initialize(EShaderType::Vertex);
			_simpleVS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_vs");
			_simplePS->Initialize(EShaderType::Pixel);
			_simplePS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_ps");

			_layout = Vulkan_CreateInputLayout();
			ColoredVertex dummyVert;
			_layout->InitializeLayout(GetVertexStreams(dummyVert));


			auto vsRef = _simpleVS->GetGPURef();
			auto psRef = _simplePS->GetGPURef();

			SE_ASSERT(vsRef && psRef);

			_state = GetVulkanPipelineState(EBlendState::Disabled,
				ERasterizerState::NoCull,
				EDepthState::Disabled,
				EDrawingTopology::LineList,
				_layout,
				vsRef,
				psRef,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr);
		}

		virtual void Shutdown()
		{
			_simpleDebugMaterial.reset();
			
			_simpleVS.reset();
			_simplePS.reset();
		}
	};

	VulkanDebugDrawing::VulkanDebugDrawing() : _impl(new DataImpl())
	{

	}
	VulkanDebugDrawing::~VulkanDebugDrawing()
	{

	}

	void VulkanDebugDrawing::Initialize(class GraphicsDevice* InOwner)
	{
		_owner = InOwner;
		_impl->Initialize(InOwner);
	}

	void VulkanDebugDrawing::Shutdown()
	{
		_impl->Shutdown();
	}

	void VulkanDebugDrawing::Draw()
	{
		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		/*
		auto vulkanMesh = std::dynamic_pointer_cast<GD_VulkanStaticMesh>(_mesh);
		auto meshPSO = _state;

		auto gpuVertexBuffer = vulkanMesh->GetVertexBuffer()->GetGPUBuffer();
		auto gpuIndexBuffer = vulkanMesh->GetIndexBuffer()->GetGPUBuffer();

		auto& vulkVB = gpuVertexBuffer->GetAs<VulkanBuffer>();
		auto& vulkIB = gpuIndexBuffer->GetAs<VulkanBuffer>();

		auto parentScene = (VulkanRenderScene*)_parentScene;
		auto cameraBuffer = parentScene->GetCameraBuffer();
		auto drawConstBuffer = parentScene->GetCameraBuffer();

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vulkVB.GetBuffer(), offsets);
		vkCmdBindIndexBuffer(commandBuffer, vulkIB.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

		auto CurPool = GGlobalVulkanGI->GetActiveDescriptorPool();

		auto& descriptorSetLayouts = meshPSO->GetDescriptorSetLayouts();
		auto setStartIdx = meshPSO->GetStartIdx();

		std::vector<VkDescriptorSet> locaDrawSets;
		locaDrawSets.resize(descriptorSetLayouts.size());

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(CurPool, descriptorSetLayouts.data(), descriptorSetLayouts.size());
		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice, &allocInfo, locaDrawSets.data()));

		//set 0
		{
			VkDescriptorBufferInfo perFrameInfo;
			perFrameInfo.buffer = cameraBuffer->GetBuffer();
			perFrameInfo.offset = 0;
			perFrameInfo.range = cameraBuffer->GetPerElementSize();

			VkDescriptorBufferInfo drawConstsInfo;
			drawConstsInfo.buffer = _drawConstantsBuffer->GetBuffer();
			drawConstsInfo.offset = 0;
			drawConstsInfo.range = _drawConstantsBuffer->GetPerElementSize();

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(locaDrawSets[0],
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
				vks::initializers::writeDescriptorSet(locaDrawSets[0],
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &drawConstsInfo),
			};

			vkUpdateDescriptorSets(vulkanDevice,
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);
		}

		uint32_t uniform_offsets[] = {
			(sizeof(GPUViewConstants)) * currentFrame,
			0
		};

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPSO->GetVkPipeline());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			meshPSO->GetVkPipelineLayout(),
			setStartIdx,
			locaDrawSets.size(), locaDrawSets.data(), ARRAY_SIZE(uniform_offsets), uniform_offsets);
		vkCmdDrawIndexed(commandBuffer, gpuIndexBuffer->GetElementCount(), 1, 0, 0, 0);
		*/
	}
	
}