// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "VulkanDebugDrawing.h"
#include "VulkanDevice.h"
#include "VulkanRenderScene.h"
#include "VulkanBuffer.h"
#include "VulkanShaders.h"
#include <chrono>

#define MAX_LINES 1500

namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	extern GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(GraphicsDevice* InOwner,
		VkFrameDataContainer& renderPassData,
		EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		EDrawingTopology InTopology,
		GPUReferencer< VulkanInputLayout > InLayout,
		GPUReferencer< VulkanShader > InVS,
		GPUReferencer< VulkanShader > InPS,
		GPUReferencer< VulkanShader > InMS,
		GPUReferencer< VulkanShader > InAS,
		GPUReferencer< VulkanShader > InHS,
		GPUReferencer< VulkanShader > InDS,
		GPUReferencer< VulkanShader > InCS );

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
		std::vector< ColoredLine > _persistentlines;
		std::shared_ptr< ArrayResource > _linesResource;
		GPUReferencer < VulkanBuffer > _lineBuffer;
		uint32_t _gpuLineCount = 0;
		GraphicsDevice* _owner = nullptr;

		std::vector< ColoredLine > _transientLines;

		DataImpl(class GraphicsDevice* InOwner) : _owner(InOwner)
		{
			_simpleVS = InOwner->CreateShader();
			_simplePS = InOwner->CreateShader();
		}

		// called on render thread
		virtual void Initialize()
		{
			_simpleVS->Initialize(EShaderType::Vertex);
			_simpleVS->CompileShaderFromFile("shaders/debugLineVS.glsl");
			_simplePS->Initialize(EShaderType::Pixel);
			_simplePS->CompileShaderFromFile("shaders/debugLinePS.glsl");

			_layout = Make_GPU(VulkanInputLayout, _owner);
			ColoredVertex dummyVert;
			_layout->InitializeLayout(GetVertexStreams(dummyVert));

			auto vsRef = _simpleVS->GetGPURef();
			auto psRef = _simplePS->GetGPURef();

			SE_ASSERT(vsRef && psRef);

			auto owningDevice = dynamic_cast<VulkanGraphicsDevice*>(_owner);


			_state = GetVulkanPipelineState(_owner,
				owningDevice->GetLightingCompositeRenderPass(),
				EBlendState::Disabled,
				ERasterizerState::NoCull,
				EDepthState::Enabled_NoWrites,
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

	void VulkanDebugDrawing::AddDebugLine(const Vector3d& Start, const Vector3d& End, const Vector3& Color, bool bTransient)
	{
		std::unique_lock<std::mutex> lock(_impl->_lineLock);
		std::vector< ColoredLine >& curLines = bTransient ? _impl->_transientLines : _impl->_persistentlines;
		curLines.push_back({ ColoredVertex{ Start.cast<float>(), Color }, ColoredVertex{ End.cast<float>(), Color } } );
		_impl->bUpdateLines |= !bTransient;
	}

	void VulkanDebugDrawing::AddDebugBox(const Vector3d& Center, const Vector3d& Extents, const Vector3& Color, bool bTransient)
	{
		auto minValue = (Center - Extents).cast<float>();
		auto maxValue = (Center + Extents).cast<float>();

		std::vector< ColoredLine >& curLines = bTransient ? _impl->_transientLines : _impl->_persistentlines;

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

			curLines.push_back({ ColoredVertex{ topPoints[Iter], Color }, ColoredVertex{ topPoints[nextPoint], Color } });
			curLines.push_back({ ColoredVertex{ bottomPoints[Iter], Color }, ColoredVertex{ bottomPoints[nextPoint], Color } });
			curLines.push_back({ ColoredVertex{ topPoints[Iter], Color }, ColoredVertex{ bottomPoints[Iter], Color } });
		}
		_impl->bUpdateLines |= !bTransient;
	}


	void VulkanDebugDrawing::AddDebugSphere(const Vector3d& Center, float Radius, const Vector3& Color, bool bTransient)
	{
		//std::vector< ColoredLine >& curLines = bTransient ? _impl->_transientLines : _impl->_persistentlines;
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
			_impl->_gpuLineCount = _impl->_persistentlines.size();
			auto updateCount = _impl->_gpuLineCount - lastLineCount;

			auto lineSpan = _impl->_linesResource->GetSpan< ColoredLine >();

			memcpy(&lineSpan[lastLineCount], &_impl->_persistentlines[lastLineCount], sizeof(ColoredLine) * updateCount);
			_impl->_lineBuffer->UpdateDirtyRegion(lastLineCount, updateCount);

			_impl->bUpdateLines = false;
		}
	}

	void VulkanDebugDrawing::Draw(VulkanRenderScene *InScene)
	{
		//if (!_impl->_gpuLineCount)
		//{
		//	return;
		//}
		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		std::unique_lock<std::mutex> lock(_impl->_lineLock);

		std::vector< ColoredLine > transientCopy;
		std::swap(transientCopy, _impl->_transientLines);

		if (!transientCopy.empty())
		{
			uint32_t lineCount = (uint32_t)transientCopy.size();
			auto curScratch = scratchBuffer.Write((const uint8_t *)transientCopy.data(), sizeof(ColoredLine) * transientCopy.size(), currentFrame);
			
			//VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
			//bufferBarrier.buffer = curScratch.buffer;
			//bufferBarrier.size = VK_WHOLE_SIZE;
			//bufferBarrier.srcAccessMask = VK_ACCESS_HOST_READ_BIT;
			//bufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			//bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			//bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			//vkCmdPipelineBarrier(
			//	commandBuffer,
			//	VK_PIPELINE_STAGE_HOST_BIT,
			//	VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			//	VK_FLAGS_NONE,
			//	0, nullptr,
			//	1, &bufferBarrier,
			//	0, nullptr);

			VkDeviceSize offsets[1] = { curScratch.offsetFromBase };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &curScratch.buffer, offsets);

			VkDescriptorSet locaDrawSets[] = { InScene->GetCommondDescriptorSet() };
			uint32_t uniform_offsets[] = { 0 };

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _impl->_state->GetVkPipeline());
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				_impl->_state->GetVkPipelineLayout(),
				0,
				ARRAY_SIZE(locaDrawSets), locaDrawSets,
				ARRAY_SIZE(uniform_offsets), uniform_offsets);
			vkCmdDraw(commandBuffer, lineCount * 2, lineCount, 0, 0);
		}
	}
	
}