// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "VulkanDebugDrawing.h"
#include "VulkanDevice.h"
#include "VulkanRenderScene.h"
#include "VulkanBuffer.h"
#include <chrono>

#define MAX_LINES 1500

namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	extern GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(GraphicsDevice* InOwner,
		EBlendState InBlendState,
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

	struct ColoredLine
	{		
		ColoredVertex start;
		ColoredVertex end;
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
	public:
		
		GPUReferencer < VulkanPipelineState > _PSO;
		std::shared_ptr< class GD_Shader > _simpleVS;
		std::shared_ptr< class GD_Shader > _simplePS;
		std::shared_ptr<GD_Material> _simpleDebugMaterial;
		GPUReferencer< GPUInputLayout > _layout;
		GPUReferencer< VulkanPipelineState > _state;
		
		std::mutex _lineLock;

		bool bUpdateLines = false;
		std::vector< ColoredLine > _lines;
		std::shared_ptr< ArrayResource > _linesResource;
		GPUReferencer < VulkanBuffer > _lineBuffer;
		uint32_t _gpuLineCount = 0;

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

			_layout = Make_GPU(VulkanInputLayout, InOwner); 
			ColoredVertex dummyVert;
			_layout->InitializeLayout(GetVertexStreams(dummyVert));

			auto vsRef = _simpleVS->GetGPURef();
			auto psRef = _simplePS->GetGPURef();

			SE_ASSERT(vsRef && psRef);

			_state = GetVulkanPipelineState(InOwner,
				EBlendState::Disabled,
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

			_linesResource = std::make_shared<ArrayResource>();
			_linesResource->InitializeFromType<ColoredLine>(MAX_LINES);
			_lineBuffer = Make_GPU(VulkanBuffer, InOwner, GPUBufferType::Vertex, _linesResource);
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

	void VulkanDebugDrawing::AddDebugLine(const Vector3d& Start, const Vector3d& End, const Vector3& Color)
	{
		std::unique_lock<std::mutex> lock(_impl->_lineLock);
		_impl->_lines.push_back({ ColoredVertex{ Start.cast<float>(), Color }, ColoredVertex{ End.cast<float>(), Color } } );
		_impl->bUpdateLines = true;
	}

	void VulkanDebugDrawing::AddDebugBox(const Vector3d& Center, const Vector3d& Extents, const Vector3& Color)
	{
		auto minValue = (Center - Extents).cast<float>();
		auto maxValue = (Center + Extents).cast<float>();

		Vector3 topPoints[4];
		Vector3 bottomPoints[4];

		topPoints[0] = Vector3(minValue[0], minValue[1], minValue[2]);
		topPoints[1] = Vector3(maxValue[0], minValue[1], minValue[2]);
		topPoints[2] = Vector3(maxValue[0], minValue[1], maxValue[2]);
		topPoints[3] = Vector3(minValue[0], minValue[1], maxValue[2]);

		bottomPoints[0] = Vector3(minValue[0], maxValue[1], minValue[2]);
		bottomPoints[1] = Vector3(maxValue[0], maxValue[1], minValue[2]);
		bottomPoints[2] = Vector3(maxValue[0], maxValue[1], maxValue[2]);
		bottomPoints[3] = Vector3(minValue[0], maxValue[1], maxValue[2]);
		
		std::unique_lock<std::mutex> lock(_impl->_lineLock);
		for (int32_t Iter = 0; Iter < 4; Iter++)
		{
			int32_t nextPoint = (Iter + 1) % 4;

			_impl->_lines.push_back({ ColoredVertex{ topPoints[Iter], Color }, ColoredVertex{ topPoints[nextPoint], Color } });
			_impl->_lines.push_back({ ColoredVertex{ bottomPoints[Iter], Color }, ColoredVertex{ bottomPoints[nextPoint], Color } });
			_impl->_lines.push_back({ ColoredVertex{ topPoints[Iter], Color }, ColoredVertex{ bottomPoints[Iter], Color } });
		}
		_impl->bUpdateLines = true;
	}


	void VulkanDebugDrawing::AddDebugSphere(const Vector3d& Center, float Radius, const Vector3& Color)
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

	void VulkanDebugDrawing::PrepareForDraw()
	{
		std::unique_lock<std::mutex> lock(_impl->_lineLock);
		if (_impl->bUpdateLines)
		{
			auto lastLineCount = _impl->_gpuLineCount;
			_impl->_gpuLineCount = _impl->_lines.size();
			auto updateCount = _impl->_gpuLineCount - lastLineCount;

			auto lineSpan = _impl->_linesResource->GetSpan< ColoredLine >();

			memcpy(&lineSpan[lastLineCount], &_impl->_lines[lastLineCount], sizeof(ColoredLine) * updateCount);
			_impl->_lineBuffer->UpdateDirtyRegion(lastLineCount, updateCount);

			_impl->bUpdateLines = false;
		}
	}

	void VulkanDebugDrawing::Draw()
	{
		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		std::unique_lock<std::mutex> lock(_impl->_lineLock);

		/*
		auto parentScene = (VulkanRenderScene*)_parentScene;
		auto cameraBuffer = parentScene->GetCameraBuffer();
		auto drawConstBuffer = parentScene->GetCameraBuffer();

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &_impl->_lineBuffer->GetBuffer(), offsets);

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
		vkCmdDraw(commandBuffer, _impl->_gpuLineCount * 2, _impl->_gpuLineCount, 0, 0);
		*/
	}
	
}