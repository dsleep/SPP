// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#pragma once

#include "SPPCore.h"
#include "SPPString.h"
#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"
#include "SPPCamera.h"

#include "VulkanTools.h"
#include "vulkan/vulkan.h"
#include "VulkanDebug.h"

#include <algorithm>
#include <assert.h>
#include <exception>


namespace SPP
{
	class VulkanGraphicsDevice;

	class SafeVkCommandBuffer : public GPUResource
	{
	private:
		VkCommandBuffer _cmdBuf = nullptr;
		VkCommandPool _owningPool = nullptr;

	public:
		SafeVkCommandBuffer(const VkCommandBufferAllocateInfo& info);
		~SafeVkCommandBuffer();
		VkCommandBuffer &Get()
		{
			return _cmdBuf;
		}
	};

	template<typename ResourceType>
	class SafeVkResource : public GPUResource
	{
	protected:
		VkDevice _vkRef = nullptr;
		ResourceType _resource = nullptr;

	public:
		SafeVkResource() 
		{
			auto vulkanDevice = dynamic_cast<VulkanGraphicsDevice*>(GGI()->GetGraphicsDevice());
			_vkRef = vulkanDevice->GetDevice();
			SE_ASSERT(_vkRef);
		}
		virtual ~SafeVkResource() { _vkRef = nullptr; }
		ResourceType& Get()
		{
			return _resource;
		}
	};

	class SafeVkFence : public SafeVkResource< VkFence >
	{	
	public:
		SafeVkFence(const VkFenceCreateInfo &info) :
			SafeVkResource< VkFence >()
		{
			VK_CHECK_RESULT(vkCreateFence(_vkRef, &info, nullptr, &_resource));
		}
		virtual ~SafeVkFence()
		{
			if (_resource)
			{
				vkDestroyFence(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkImage : public SafeVkResource< VkImage >
	{
	public:
		SafeVkImage(const VkImageCreateInfo& info) :
			SafeVkResource< VkImage >()
		{
			VK_CHECK_RESULT(vkCreateImage(_vkRef, &info, nullptr, &_resource));
			vks::debugmarker::setImageName(_vkRef, _resource, "SafeVkImage_UNSET");
		}
		virtual ~SafeVkImage()
		{
			if (_resource)
			{
				vkDestroyImage(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkDeviceMemory : public SafeVkResource< VkDeviceMemory >
	{
	public:
		SafeVkDeviceMemory(const VkMemoryAllocateInfo& info) :
			SafeVkResource< VkDeviceMemory >()
		{
			SE_ASSERT(_resource == nullptr);
			VK_CHECK_RESULT(vkAllocateMemory(_vkRef, &info, nullptr, &_resource));
		}
		virtual ~SafeVkDeviceMemory()
		{
			if (_resource)
			{
				vkFreeMemory(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
			_vkRef = nullptr;
		}
		VkDeviceMemory& Get()
		{
			return _resource;
		}
	};

	class SafeVkImageView : public SafeVkResource< VkImageView >
	{
	public:
		SafeVkImageView(const VkImageViewCreateInfo& info) :
			SafeVkResource< VkImageView >()
		{
			SE_ASSERT(_resource == nullptr);
			VK_CHECK_RESULT(vkCreateImageView(_vkRef, &info, nullptr, &_resource));
		}
		virtual ~SafeVkImageView()
		{
			if (_resource)
			{
				vkDestroyImageView(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
		}
		VkImageView& Get()
		{
			return _resource;
		}
	};

	class SafeVkFrameBuffer : public SafeVkResource< VkFramebuffer >
	{
	public:
		SafeVkFrameBuffer(const VkFramebufferCreateInfo& info) :
			SafeVkResource< VkFramebuffer >()
		{
			VK_CHECK_RESULT(vkCreateFramebuffer(_vkRef, &info, nullptr, &_resource));
		}
		virtual ~SafeVkFrameBuffer()
		{
			if (_resource)
			{
				vkDestroyFramebuffer(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkRenderPass : public SafeVkResource< VkRenderPass >
	{
	public:
		SafeVkRenderPass(const VkRenderPassCreateInfo& info) :
			SafeVkResource< VkRenderPass >()
		{
			VK_CHECK_RESULT(vkCreateRenderPass(_vkRef, &info, nullptr, &_resource));
		}
		virtual ~SafeVkRenderPass()
		{
			if (_resource)
			{
				vkDestroyRenderPass(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkSampler : public SafeVkResource< VkSampler >
	{
	public:
		SafeVkSampler(const VkSamplerCreateInfo& info) :
			SafeVkResource< VkSampler >()
		{
			VK_CHECK_RESULT(vkCreateSampler(_vkRef, &info, nullptr, &_resource));
		}
		virtual ~SafeVkSampler()
		{
			if (_resource)
			{
				vkDestroySampler(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	struct SafeVkCommandAndFence
	{
		GPUReferencer<SafeVkFence> fence;
		GPUReferencer<SafeVkCommandBuffer> cmdBuf;
		bool bHasBegun = false;
	};	


	class SafeVkDescriptorSetLayout : public SafeVkResource< VkDescriptorSetLayout >
	{
	public:
		SafeVkDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) :
			SafeVkResource< VkDescriptorSetLayout >()
		{
			VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(bindings);
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_vkRef, &descriptorLayout, nullptr, &_resource));
		}
		SafeVkDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo &Info) :
			SafeVkResource< VkDescriptorSetLayout >()
		{
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_vkRef, &Info, nullptr, &_resource));
		}

		virtual ~SafeVkDescriptorSetLayout()
		{
			if (_resource)
			{
				vkDestroyDescriptorSetLayout(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	//
	class SafeVkDescriptorSet : public SafeVkResource< VkDescriptorSet >
	{
	protected:
		VkDescriptorPool _usedPool = nullptr;

	public:
		SafeVkDescriptorSet(const VkDescriptorSetLayout& info, VkDescriptorPool InPool) :
			SafeVkResource< VkDescriptorSet >(), _usedPool(InPool)
		{
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(_usedPool, &info, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(_vkRef, &allocInfo, &_resource));
		}

		struct UpdateDescriptor
		{
			VkDescriptorType type;
			uint32_t binding;

			VkDescriptorBufferInfo* bufferInfo = nullptr;
			VkDescriptorImageInfo* imageInfo = nullptr;
			
			UpdateDescriptor(VkDescriptorType InType, uint32_t InBinding, VkDescriptorBufferInfo *InInfo)
			{
				type = InType;
				binding = InBinding;
				bufferInfo = InInfo;
			}

			UpdateDescriptor(VkDescriptorType InType, uint32_t InBinding, VkDescriptorImageInfo* InInfo)
			{
				type = InType;
				binding = InBinding;
				imageInfo = InInfo;
			}
		};

		void Update(const std::vector< UpdateDescriptor > &InData)
		{
			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			writeDescriptorSets.resize(InData.size());
			for (size_t Iter = 0; Iter < InData.size(); Iter++)
			{
				VkWriteDescriptorSet writeDescriptorSet{};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.dstSet = _resource;
				writeDescriptorSet.descriptorType = InData[Iter].type;
				writeDescriptorSet.dstBinding = InData[Iter].binding;
				writeDescriptorSet.pImageInfo = InData[Iter].imageInfo;
				writeDescriptorSet.pBufferInfo = InData[Iter].bufferInfo;
				writeDescriptorSet.descriptorCount = 1;

				writeDescriptorSets[Iter] = writeDescriptorSet;
			}
			vkUpdateDescriptorSets(_vkRef,
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);
		}

		virtual ~SafeVkDescriptorSet()
		{
			if (_resource)
			{
				vkFreeDescriptorSets(_vkRef, _usedPool, 1, &_resource);
				_resource = nullptr;
			}
		}
	};

	class SafeVkPipelineLayout : public SafeVkResource< VkPipelineLayout >
	{
	public:
		SafeVkPipelineLayout(const VkPipelineLayoutCreateInfo& info) :
			SafeVkResource< VkPipelineLayout >()
		{
			VK_CHECK_RESULT(vkCreatePipelineLayout(_vkRef, &info, nullptr, &_resource));
		}

		virtual ~SafeVkPipelineLayout()
		{
			if (_resource)
			{
				vkDestroyPipelineLayout(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkPipeline : public SafeVkResource< VkPipeline >
	{
	public:
		SafeVkPipeline(const VkGraphicsPipelineCreateInfo& info) :
			SafeVkResource< VkPipeline >()
		{
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(_vkRef, nullptr, 1, &info, nullptr, &_resource));
		}

		SafeVkPipeline(const VkComputePipelineCreateInfo& info) :
			SafeVkResource< VkPipeline >()
		{
			VK_CHECK_RESULT(vkCreateComputePipelines(_vkRef, nullptr, 1, &info, nullptr, &_resource));
		}

		virtual ~SafeVkPipeline()
		{
			if (_resource)
			{
				vkDestroyPipeline(_vkRef, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};
}   
