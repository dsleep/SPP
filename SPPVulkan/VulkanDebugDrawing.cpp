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
		std::shared_ptr< class RT_Shader > _simpleVS;
		std::shared_ptr< class RT_Shader > _simplePS;
		GPUReferencer< GPUInputLayout > _layout;
		GPUReferencer< VulkanPipelineState > _state;
		
		std::mutex _lineLock;

		bool bUpdateLines = false;
		std::vector< ColoredLine > _lines;
		std::shared_ptr< ArrayResource > _linesResource;
		GPUReferencer < VulkanBuffer > _lineBuffer;
		uint32_t _gpuLineCount = 0;
		GraphicsDevice* _owner = nullptr;

		DataImpl(class GraphicsDevice* InOwner) : _owner(InOwner)
		{
			_simpleVS = InOwner->CreateShader();
			_simplePS = InOwner->CreateShader();
		}

		// called on render thread
		virtual void Initialize()
		{
			_simpleVS->Initialize(EShaderType::Vertex);
			_simpleVS->CompileShaderFromFile("shaders/debugLine.hlsl", "main_vs");
			_simplePS->Initialize(EShaderType::Pixel);
			_simplePS->CompileShaderFromFile("shaders/debugLine.hlsl", "main_ps");

			_layout = Make_GPU(VulkanInputLayout, _owner);
			ColoredVertex dummyVert;
			_layout->InitializeLayout(GetVertexStreams(dummyVert));

			auto vsRef = _simpleVS->GetGPURef();
			auto psRef = _simplePS->GetGPURef();

			SE_ASSERT(vsRef && psRef);

			_state = GetVulkanPipelineState(_owner,
				EBlendState::Disabled,
				ERasterizerState::NoCull,
				EDepthState::Enabled,
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
			_lineBuffer = Make_GPU(VulkanBuffer, _owner, GPUBufferType::Vertex, _linesResource);
		}

		virtual void Shutdown()
		{			
			_simpleVS.reset();
			_simplePS.reset();

			_linesResource.reset();
			_lineBuffer.Reset();
		}
	};

	VulkanDebugDrawing::VulkanDebugDrawing(GraphicsDevice* InOwner) : _impl(new DataImpl(InOwner))
	{
		_owner = InOwner;
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

	void VulkanDebugDrawing::Initialize()
	{
		_impl->Initialize();
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

	void VulkanDebugDrawing::Draw(VulkanRenderScene *InScene)
	{
		if (!_impl->_gpuLineCount)
		{
			return;
		}
		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		std::unique_lock<std::mutex> lock(_impl->_lineLock);

		auto cameraBuffer = InScene->GetCameraBuffer();

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &_impl->_lineBuffer->GetBuffer(), offsets);

		auto CurPool = GGlobalVulkanGI->GetPerFrameResetDescriptorPool();

		auto& descriptorSetLayouts = _impl->_state->GetDescriptorSetLayouts();
		auto setStartIdx = _impl->_state->GetStartIdx();

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

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(locaDrawSets[0],
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo)
			};

			vkUpdateDescriptorSets(vulkanDevice,
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);
		}

		uint32_t uniform_offsets[] = {
			(sizeof(GPUViewConstants)) * currentFrame
		};

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _impl->_state->GetVkPipeline());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			_impl->_state->GetVkPipelineLayout(),
			setStartIdx,
			locaDrawSets.size(), locaDrawSets.data(), ARRAY_SIZE(uniform_offsets), uniform_offsets);
		vkCmdDraw(commandBuffer, _impl->_gpuLineCount * 2, _impl->_gpuLineCount, 0, 0);
	}
	
}