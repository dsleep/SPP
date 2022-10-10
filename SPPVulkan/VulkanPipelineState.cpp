// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "VulkanPipelineState.h"
#include "VulkanTexture.h"
#include "VulkanShaders.h"
#include "VulkanDebug.h"
#include "VulkanFrameBuffer.hpp"
#include "VulkanDebugDrawing.h"
#include "VulkanRenderScene.h"
#include "SPPGraphics.h"
#include "ThreadPool.h"

#include <unordered_set>
#include "SPPLogging.h"
#include "SPPSceneRendering.h"
#include "SPPGraphicsO.h"
#include "SPPSDFO.h"

#include "vulkan/vulkan_win32.h"

namespace SPP
{
	extern LogEntry LOG_VULKAN;

	struct VulkanPipelineStateBuilder::Impl
	{
		VkFrameDataContainer renderPassData = {};

		EBlendState blendState = EBlendState::Disabled;
		ERasterizerState rasterizerState = ERasterizerState::NoCull;
		EDepthState depthState = EDepthState::Disabled;
		EDrawingTopology drawingTopology = EDrawingTopology::TriangleList;
		EDepthOp depthOp = EDepthOp::Always;

		GPUReferencer < VulkanInputLayout > inputLayout;

		std::map< EShaderType, GPUReferencer < VulkanShader > > shaderMap;

		std::vector< VkDynamicState > dynamicStates;
	};

	VulkanPipelineStateBuilder::VulkanPipelineStateBuilder(GraphicsDevice* InOwner) : _impl(new Impl()), _owner(InOwner)
	{
	}

	VulkanPipelineStateBuilder::~VulkanPipelineStateBuilder()
	{

	}

	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(VkFrameDataContainer& InValue)
	{
		_impl->renderPassData = InValue;
		return *this;
	}

	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(EBlendState InValue)
	{
		_impl->blendState = InValue;
		return *this;
	}
	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(ERasterizerState InValue)
	{
		_impl->rasterizerState = InValue;
		return *this;
	}
	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(EDepthState InValue)
	{
		_impl->depthState = InValue;
		return *this;
	}
	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(EDrawingTopology InValue)
	{
		_impl->drawingTopology = InValue;
		return *this;
	}
	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(EDepthOp InValue)
	{
		_impl->depthOp = InValue;
		return *this;
	}
	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(GPUReferencer<GPUShader> InValue)
	{
		_impl->shaderMap[InValue->GetType()] = InValue;
		return *this;
	}
	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(GPUReferencer<class VulkanShader> InValue)
	{
		_impl->shaderMap[InValue->GetType()] = InValue;
		return *this;
	}

	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(GPUReferencer<GPUInputLayout > InValue)
	{
		_impl->inputLayout = InValue;
		return *this;
	}	
	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(GPUReferencer<class VulkanInputLayout> InValue)
	{
		_impl->inputLayout = InValue;
		return *this;
	}

	VulkanPipelineStateBuilder& VulkanPipelineStateBuilder::Set(VkDynamicState InValue)
	{
		_impl->dynamicStates.push_back(InValue);
		return *this;
	}

	


	VulkanPipelineState::VulkanPipelineState(GraphicsDevice* InOwner) : PipelineState(InOwner)
	{
	}

	VulkanPipelineState::~VulkanPipelineState()
	{
	}


	/// <summary>
	/// 
	/// </summary>
	/// <param name="InDescriptorSet"></param>
	/// <param name="oSetLayoutBindings"></param>
	void MergeBindingSet(const std::vector<DescriptorSetLayoutData>& InDescriptorSet,
		std::map<uint8_t, std::vector<VkDescriptorSetLayoutBinding> > &oSetLayoutBindings)
	{
		for (auto& curSet : InDescriptorSet)
		{
			auto foundEle = oSetLayoutBindings.find(curSet.set_number);
			if (foundEle != oSetLayoutBindings.end())
			{
				std::vector<VkDescriptorSetLayoutBinding> &setLayoutBindings = foundEle->second;

				for (auto& newBinding : curSet.bindings)
				{
					bool bDoAdd = true;

					for (auto& curBinding : setLayoutBindings)
					{
						if (curBinding.binding == newBinding.binding)
						{
							curBinding.stageFlags |= newBinding.stageFlags;
							
							SE_ASSERT(curBinding.descriptorCount == curBinding.descriptorCount
								&& curBinding.descriptorType == curBinding.descriptorType);

							bDoAdd = false;
							break;
						}
					}

					if (bDoAdd)
					{
						setLayoutBindings.insert(setLayoutBindings.end(), newBinding);
					}
				}
			}
			else
			{
				oSetLayoutBindings[(uint8_t)curSet.set_number] = curSet.bindings;
			}
		}
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="Idx"></param>
	/// <param name="InPool"></param>
	/// <returns></returns>
	GPUReferencer<SafeVkDescriptorSet> VulkanPipelineState::CreateDescriptorSet(uint8_t Idx, VkDescriptorPool InPool) const
	{
		return Make_GPU(SafeVkDescriptorSet,
			_owner,
			_descriptorSetLayouts[Idx]->Get(),
			InPool);
	}

	const VkPipeline& VulkanPipelineState::GetVkPipeline()
	{
		return _pipeline->Get();
	}

	const VkPipelineLayout& VulkanPipelineState::GetVkPipelineLayout()
	{
		return _pipelineLayout->Get();
	}

	std::vector< VkDescriptorSetLayout > VulkanPipelineState::GetDescriptorSetLayoutsDirect()
	{
		return SafeArrayToResources< VkDescriptorSetLayout, decltype(_descriptorSetLayouts) >(_descriptorSetLayouts);
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="renderPassData"></param>
	/// <param name="InBlendState"></param>
	/// <param name="InRasterizerState"></param>
	/// <param name="InDepthState"></param>
	/// <param name="InTopology"></param>
	/// <param name="InDepthOp"></param>
	/// <param name="InLayout"></param>
	/// <param name="InVS"></param>
	/// <param name="InPS"></param>
	/// <param name="InMS"></param>
	/// <param name="InAS"></param>
	/// <param name="InHS"></param>
	/// <param name="InDS"></param>
	/// <param name="InCS"></param>
	void VulkanPipelineState::Initialize(VkFrameDataContainer & renderPassData,

		EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		EDrawingTopology InTopology,

		EDepthOp InDepthOp,

		GPUReferencer < GPUInputLayout > InLayout,
		GPUReferencer< GPUShader> InVS,
		GPUReferencer< GPUShader> InPS,

		GPUReferencer< GPUShader> InMS,
		GPUReferencer< GPUShader> InAS,
		GPUReferencer< GPUShader> InHS,
		GPUReferencer< GPUShader> InDS,

		GPUReferencer< GPUShader> InCS,
		
		const std::vector< VkDynamicState >& InExtraStates)
	{
		auto owningDevice = dynamic_cast<VulkanGraphicsDevice*>(_owner);
		auto device = owningDevice->GetVKDevice();
		
		if (InVS)
		{
			auto renderPass = renderPassData.renderPass->Get();

			VulkanInputLayout* inputLayout = InLayout ? &InLayout->GetAs<VulkanInputLayout>() : nullptr;

			// Shaders
			std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

			auto& vsSet = InVS->GetAs<VulkanShader>().GetLayoutSets();
			
			// Deferred shading layout
			_setLayoutBindings.clear();

			MergeBindingSet(vsSet, _setLayoutBindings);

			if (InPS)
			{
				auto& psSet = InPS->GetAs<VulkanShader>().GetLayoutSets();
				MergeBindingSet(psSet, _setLayoutBindings);
			}

			SE_ASSERT(_descriptorSetLayouts.empty());
			_descriptorSetLayouts.resize(4);

			VkDescriptorSetLayout descriptorSetLayoutsRefs[4] = {};

			// step through sets numerically
			for (int32_t Iter = 0; Iter < 4; Iter++)
			{
				auto foundSet = _setLayoutBindings.find(Iter);
				if(foundSet != _setLayoutBindings.end())
				{
					VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(foundSet->second);
					_descriptorSetLayouts[Iter] = std::make_unique< SafeVkDescriptorSetLayout >(owningDevice, descriptorLayout);
				}
				// else create dummy
				else
				{
					VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(nullptr, 0);
					_descriptorSetLayouts[Iter] = std::make_unique< SafeVkDescriptorSetLayout >(owningDevice, descriptorLayout);
				}
				descriptorSetLayoutsRefs[Iter] = _descriptorSetLayouts[Iter]->Get();
			}

			SE_ASSERT(InVS->GetAs<VulkanShader>().GetModule());

			shaderStages.push_back({ 
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				nullptr,
				0,//TODO this worth it?VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT,
				VK_SHADER_STAGE_VERTEX_BIT,
				InVS->GetAs<VulkanShader>().GetModule(),
				InVS->GetAs<VulkanShader>().GetEntryPoint().c_str()
				});

			if (InPS)
			{
				SE_ASSERT(InPS->GetAs<VulkanShader>().GetModule());

				shaderStages.push_back({
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					nullptr,
					0,//TODO this worth it?VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					InPS->GetAs<VulkanShader>().GetModule(),
					InPS->GetAs<VulkanShader>().GetEntryPoint().c_str()
				});
			}

			//TODO setup ps/vs merge
			// setup push constants
			std::vector<VkPushConstantRange> push_constants;

			//TODO add VS MERGE!!
			if (InPS)
			{
				push_constants = InPS->GetAs<VulkanShader>().GetPushConstants();
			}

			////this push constant range starts at the beginning
			//push_constant.offset = 0;
			////this push constant range takes up the size of a MeshPushConstants struct
			//push_constant.size = sizeof(MeshPushConstants);
			////this push constant range is accessible only in the vertex shader
			//push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

			//pipelineCreateInfo.pPushConstantRanges = &push_constant;
			//pipelineCreateInfo.pushConstantRangeCount = 1;
			
			
			// Create the pipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
			// In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused
			VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
			pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pPipelineLayoutCreateInfo.pNext = nullptr;
			pPipelineLayoutCreateInfo.setLayoutCount = ARRAY_SIZE(descriptorSetLayoutsRefs);
			pPipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayoutsRefs;

			pPipelineLayoutCreateInfo.pushConstantRangeCount = push_constants.size();
			pPipelineLayoutCreateInfo.pPushConstantRanges = push_constants.data();

			_pipelineLayout = std::make_unique< SafeVkPipelineLayout >(owningDevice, pPipelineLayoutCreateInfo);

			// Vulkan uses the concept of rendering pipelines to encapsulate fixed states, replacing OpenGL's complex state machine
			// A pipeline is then stored and hashed on the GPU making pipeline changes very fast
			// Note: There are still a few dynamic states that are not directly part of the pipeline (but the info that they are used is)

			VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			// The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
			pipelineCreateInfo.layout = _pipelineLayout->Get();
			// Renderpass this pipeline is attached to
			pipelineCreateInfo.renderPass = renderPass; 

			// Construct the different states making up the pipeline

			// Input assembly state describes how primitives are assembled
			// This pipeline will assemble vertex data as a triangle lists (though we only use one triangle)
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
			inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

			switch (InTopology)
			{
			case EDrawingTopology::TriangleList:
				inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				break;
			case EDrawingTopology::TriangleStrip:
				inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				break;
			case EDrawingTopology::LineList:
				inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
				break;
			case EDrawingTopology::PointList:
				inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
				break;
			default:
				SE_ASSERT(false);
			}

			// Rasterization state
			VkPipelineRasterizationStateCreateInfo rasterizationState = {};
			rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationState.cullMode = VK_CULL_MODE_NONE;
			rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizationState.depthClampEnable = VK_FALSE;
			rasterizationState.rasterizerDiscardEnable = VK_FALSE;
			rasterizationState.depthBiasEnable = VK_FALSE;
			rasterizationState.lineWidth = 1.0f;

			switch (InRasterizerState)
			{
			case ERasterizerState::NoCull:
				rasterizationState.cullMode = VK_CULL_MODE_NONE;
				break;
			case ERasterizerState::BackFaceCull:
				rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
				break;
			case ERasterizerState::FrontFaceCull:
				rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
				break;
			case ERasterizerState::Wireframe:
				rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
				break;
			};

			// Color blend state describes how blend factors are calculated (if used)
			// We need one blend attachment state per color attachment (even if blending is not used)
			std::vector< VkPipelineColorBlendAttachmentState > blendAttachmentStates;
			
			for (int32_t Iter = 0; Iter < renderPassData.ColorTargets; Iter++)
			{
				VkPipelineColorBlendAttachmentState blendAttachmentState = {};
				blendAttachmentState.blendEnable = VK_FALSE;
				blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

				//ugly hack for deferred
				if (Iter == 0)
				{
					switch (InBlendState)
					{
					case EBlendState::Additive:
						blendAttachmentState.blendEnable = VK_TRUE;
						blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
						blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
						blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
						blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
						blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
						blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
						blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
						break;
					case EBlendState::AlphaBlend:
						blendAttachmentState.blendEnable = VK_TRUE;
						blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
						blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
						blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
						blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
						blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
						blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
						blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
						break;
					}
				}

				blendAttachmentStates.push_back(blendAttachmentState);
			}
			

			VkPipelineColorBlendStateCreateInfo colorBlendState = {};
			colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendState.attachmentCount = blendAttachmentStates.size();
			colorBlendState.pAttachments = blendAttachmentStates.data();

			// Viewport state sets the number of viewports and scissor used in this pipeline
			// Note: This is actually overridden by the dynamic states (see below)
			VkPipelineViewportStateCreateInfo viewportState = {};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;

			// Multi sampling state
			// This example does not make use of multi sampling (for anti-aliasing), the state must still be set and passed to the pipeline
			VkPipelineMultisampleStateCreateInfo multisampleState = {};
			multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampleState.pSampleMask = nullptr;

			// Enable dynamic states
			// Most states are baked into the pipeline, but there are still a few dynamic states that can be changed within a command buffer
			// To be able to change these we need do specify which dynamic states will be changed using this pipeline. Their actual states are set later on in the command buffer.
			// For this example we will set the viewport and scissor using dynamic states
			std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			dynamicStateEnables.insert(dynamicStateEnables.end(), InExtraStates.begin(), InExtraStates.end());

			//VK_DYNAMIC_STATE_DEPTH_BOUNDS
			VkPipelineDynamicStateCreateInfo dynamicState = {};
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.pDynamicStates = dynamicStateEnables.data();
			dynamicState.dynamicStateCount = (uint32_t) dynamicStateEnables.size();

			// Depth and stencil state containing depth and stencil compare and test operations
			// We only use depth tests and want depth tests and writes to be enabled and compare with less or equal
			VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
			depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilState.depthTestEnable = VK_TRUE;
			depthStencilState.depthWriteEnable = VK_TRUE;
			//TODO check
			//VK_COMPARE_OP_GREATER or VK_COMPARE_OP_GREATER_OR_EQUAL
			depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; //normal Z VK_COMPARE_OP_LESS_OR_EQUAL

			switch (InDepthOp)
			{
			case EDepthOp::Never:
				depthStencilState.depthCompareOp = VK_COMPARE_OP_NEVER;
				break;
			case EDepthOp::Less:
				depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
				break;
			case EDepthOp::Equal:
				depthStencilState.depthCompareOp = VK_COMPARE_OP_EQUAL;
				break;
			case EDepthOp::LessOrEqual:
				depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
				break;
			case EDepthOp::Greater:
				depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER;
				break;
			case EDepthOp::NotEqual:
				depthStencilState.depthCompareOp = VK_COMPARE_OP_NOT_EQUAL;
				break;
			case EDepthOp::GreaterOrEqual:
				depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
				break;
			case EDepthOp::Always:
				depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
				break;
			default:
				SE_ASSERT(false);
			}

			depthStencilState.depthBoundsTestEnable = VK_FALSE;
			depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
			depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
			depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
			depthStencilState.stencilTestEnable = VK_FALSE;
			depthStencilState.front = depthStencilState.back;

			switch (InDepthState)
			{
			case EDepthState::Disabled:
				depthStencilState.depthTestEnable = VK_FALSE;
				depthStencilState.depthWriteEnable = VK_FALSE;
				break;
			case EDepthState::Enabled:
				depthStencilState.depthTestEnable = VK_TRUE;
				depthStencilState.depthWriteEnable = VK_TRUE;
				break;
			case EDepthState::Enabled_NoWrites:
				depthStencilState.depthTestEnable = VK_TRUE;
				depthStencilState.depthWriteEnable = VK_FALSE;
				break;
			}

			// Set pipeline shader stage info
			pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			pipelineCreateInfo.pStages = shaderStages.data();

			VkPipelineVertexInputStateCreateInfo dummy = vks::initializers::pipelineVertexInputStateCreateInfo();

			// Assign the pipeline states to the pipeline creation info structure
			pipelineCreateInfo.pVertexInputState = inputLayout ? &inputLayout->GetVertexInputState() : &dummy;
			pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
			pipelineCreateInfo.pRasterizationState = &rasterizationState;
			pipelineCreateInfo.pColorBlendState = &colorBlendState;
			pipelineCreateInfo.pMultisampleState = &multisampleState;
			pipelineCreateInfo.pViewportState = &viewportState;
			pipelineCreateInfo.pDepthStencilState = &depthStencilState;
			pipelineCreateInfo.renderPass = renderPass;
			pipelineCreateInfo.pDynamicState = &dynamicState;

			// Pipeline cache object
			//VkPipelineCache pipelineCache;
			//VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
			//pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			//VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

			// Create rendering pipeline using the specified states
			_pipeline = std::make_unique< SafeVkPipeline >(owningDevice, pipelineCreateInfo);
			//VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &_pipeline));
		}
		else if(InCS)
		{
			auto& csVulkanShader = InCS->GetAs<VulkanShader>();
			auto& csShaderSets = csVulkanShader.GetLayoutSets();

			MergeBindingSet(csShaderSets, _setLayoutBindings);

			SE_ASSERT(_descriptorSetLayouts.empty());
			_descriptorSetLayouts.resize(4);


			VkDescriptorSetLayout descriptorSetLayoutsRefs[4] = {};

			// step through sets numerically
			for (int32_t Iter = 0; Iter < 4; Iter++)
			{
				auto foundSet = _setLayoutBindings.find(Iter);
				if (foundSet != _setLayoutBindings.end())
				{
					VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(foundSet->second);

					_descriptorSetLayouts[Iter] = std::make_unique< SafeVkDescriptorSetLayout >(owningDevice, descriptorLayout);
					//VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &_descriptorSetLayouts[Iter]));
				}
				// else create dummy
				else
				{
					VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(nullptr, 0);
					//VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &_descriptorSetLayouts[Iter]));

					_descriptorSetLayouts[Iter] = std::make_unique< SafeVkDescriptorSetLayout >(owningDevice, descriptorLayout);
				}

				descriptorSetLayoutsRefs[Iter] = _descriptorSetLayouts[Iter]->Get();
			}

			// setup push constants
			std::vector<VkPushConstantRange> push_constants = csVulkanShader.GetPushConstants();

			VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
			pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pPipelineLayoutCreateInfo.pNext = nullptr;
			pPipelineLayoutCreateInfo.setLayoutCount = ARRAY_SIZE(descriptorSetLayoutsRefs);
			pPipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayoutsRefs;
			pPipelineLayoutCreateInfo.pushConstantRangeCount = push_constants.size();
			pPipelineLayoutCreateInfo.pPushConstantRanges = push_constants.data();

			//VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &_pipelineLayout));

			_pipelineLayout = std::make_unique< SafeVkPipelineLayout >(owningDevice, pPipelineLayoutCreateInfo);

			VkComputePipelineCreateInfo computePipelineCreateInfo =	vks::initializers::computePipelineCreateInfo(_pipelineLayout->Get(), 0);
			computePipelineCreateInfo.stage =
				{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				nullptr,
				0,//TODO this worth it? VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				InCS->GetAs<VulkanShader>().GetModule(),
				InCS->GetAs<VulkanShader>().GetEntryPoint().c_str()
				};

			//VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &_pipeline));
			_pipeline = std::make_unique< SafeVkPipeline >(owningDevice, computePipelineCreateInfo);
		}
		else
		{
			SE_ASSERT(false);
		}		
	}


	bool VulkanPipelineStateKey::operator<(const VulkanPipelineStateKey& compareKey)const
	{
		if (blendState != compareKey.blendState)
		{
			return blendState < compareKey.blendState;
		}
		if (rasterizerState != compareKey.rasterizerState)
		{
			return rasterizerState < compareKey.rasterizerState;
		}
		if (depthState != compareKey.depthState)
		{
			return depthState < compareKey.depthState;
		}
		if (topology != compareKey.topology)
		{
			return topology < compareKey.topology;
		}
		if (depthOp != compareKey.depthOp)
		{
			return depthOp < compareKey.depthOp;
		}		

		if (inputLayout != compareKey.inputLayout)
		{
			return inputLayout < compareKey.inputLayout;
		}

		if (vs != compareKey.vs)
		{
			return vs < compareKey.vs;
		}
		if (ps != compareKey.ps)
		{
			return ps < compareKey.ps;
		}
		if (ms != compareKey.ms)
		{
			return ms < compareKey.ms;
		}
		if (as != compareKey.as)
		{
			return as < compareKey.as;
		}
		if (hs != compareKey.hs)
		{
			return hs < compareKey.hs;
		}
		if (ds != compareKey.ds)
		{
			return ds < compareKey.ds;
		}
		if (cs != compareKey.cs)
		{
			return cs < compareKey.cs;
		}

		return false;
	}

	

	GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(GraphicsDevice* InOwner,
		VkFrameDataContainer& renderPassData,
		EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		EDrawingTopology InTopology,
		EDepthOp InDepthOp,

		GPUReferencer< VulkanInputLayout > InLayout,
		GPUReferencer< VulkanShader > InVS,
		GPUReferencer< VulkanShader > InPS,
		GPUReferencer< VulkanShader > InMS,
		GPUReferencer< VulkanShader > InAS,
		GPUReferencer< VulkanShader > InHS,
		GPUReferencer< VulkanShader > InDS,
		GPUReferencer< VulkanShader > InCS,
		
		const std::vector< VkDynamicState >& InExtraStates)
	{
		VulkanPipelineStateKey key{ InBlendState, InRasterizerState, InDepthState, InTopology, InDepthOp,
			(uintptr_t)InLayout.get(),
			(uintptr_t)InVS.get(),
			(uintptr_t)InPS.get(),
			(uintptr_t)InMS.get(),
			(uintptr_t)InAS.get(),
			(uintptr_t)InHS.get(),
			(uintptr_t)InDS.get(),
			(uintptr_t)InCS.get() };

		auto vulkanGraphicsDevice = dynamic_cast<VulkanGraphicsDevice*>(InOwner);
		auto& PipelineStateMap = vulkanGraphicsDevice->GetPipelineStateMap();

		auto findKey = PipelineStateMap.find(key);

		if (findKey == PipelineStateMap.end())
		{
			auto newPipelineState = Make_GPU(VulkanPipelineState, InOwner);
			newPipelineState->Initialize(renderPassData, InBlendState, InRasterizerState, InDepthState, InTopology, InDepthOp, InLayout, InVS, InPS, InMS, InAS, InHS, InDS, InCS);
			PipelineStateMap[key] = newPipelineState;
			return newPipelineState;
		}

		return findKey->second;
	}

	GPUReferencer < VulkanPipelineState >  GetVulkanPipelineStateWithMap(GraphicsDevice* InOwner,
		struct VkFrameDataContainer& renderPassData,

		EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		EDrawingTopology InTopology,
		EDepthOp InDepthOp,

		GPUReferencer< class VulkanInputLayout > InLayout,

		const std::map< EShaderType, GPUReferencer < VulkanShader > >& shaderMap,

		const std::vector< VkDynamicState >& InExtraStates)
	{
		GPUReferencer< VulkanShader > InVS = MapFindOrDefault(shaderMap, EShaderType::Vertex);
		GPUReferencer< VulkanShader > InPS = MapFindOrDefault(shaderMap, EShaderType::Pixel);
		GPUReferencer< VulkanShader > InCS = MapFindOrDefault(shaderMap, EShaderType::Compute);

		return GetVulkanPipelineState(InOwner,
			renderPassData,
			InBlendState,
			InRasterizerState,
			InDepthState,
			InTopology,
			InDepthOp,
			InLayout, InVS, InPS, nullptr, nullptr, nullptr, nullptr, InCS, InExtraStates);
	}


	GPUReferencer< VulkanPipelineState > VulkanPipelineStateBuilder::Build()
	{
		return GetVulkanPipelineStateWithMap(_owner,
			_impl->renderPassData,

			_impl->blendState,
			_impl->rasterizerState,
			_impl->depthState,
			_impl->drawingTopology,
			_impl->depthOp,

			_impl->inputLayout,

			_impl->shaderMap,

			_impl->dynamicStates);
	}
}
