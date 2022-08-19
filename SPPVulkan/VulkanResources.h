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

#include <algorithm>
#include <assert.h>
#include <exception>


namespace SPP
{
	class VulkanGraphicsDevice;

	class SafeVkCommandBuffer : public GPUResource
	{
	private:
		VkDevice _owningDevice = nullptr;
		VkCommandBuffer _cmdBuf = nullptr;
		VkCommandPool _owningPool = nullptr;

	public:
		SafeVkCommandBuffer(GraphicsDevice* InOwner, const VkCommandBufferAllocateInfo& info);
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
		VkDevice _owningDevice = nullptr;
		ResourceType _resource = nullptr;

	public:
		SafeVkResource(GraphicsDevice* InOwner) : GPUResource(InOwner)
		{
			auto vulkanDevice = dynamic_cast<VulkanGraphicsDevice*>(InOwner);
			_owningDevice = vulkanDevice->GetDevice();
			SE_ASSERT(_owningDevice);
		}
		virtual ~SafeVkResource() { _owningDevice = nullptr; }
		ResourceType& Get()
		{
			return _resource;
		}
	};

	class SafeVkFence : public SafeVkResource< VkFence >
	{	
	public:
		SafeVkFence(GraphicsDevice* InOwner, const VkFenceCreateInfo &info) :
			SafeVkResource< VkFence >(InOwner)
		{
			VK_CHECK_RESULT(vkCreateFence(_owningDevice, &info, nullptr, &_resource));
		}
		virtual ~SafeVkFence()
		{
			if (_resource)
			{
				vkDestroyFence(_owningDevice, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkImage : public SafeVkResource< VkImage >
	{
	public:
		SafeVkImage(GraphicsDevice* InOwner, const VkImageCreateInfo& info) :
			SafeVkResource< VkImage >(InOwner)
		{
			VK_CHECK_RESULT(vkCreateImage(_owningDevice, &info, nullptr, &_resource));
		}
		virtual ~SafeVkImage()
		{
			if (_resource)
			{
				vkDestroyImage(_owningDevice, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkDeviceMemory : public SafeVkResource< VkDeviceMemory >
	{
	public:
		SafeVkDeviceMemory(GraphicsDevice* InOwner, const VkMemoryAllocateInfo& info) :
			SafeVkResource< VkDeviceMemory >(InOwner)
		{
			SE_ASSERT(_resource == nullptr);
			VK_CHECK_RESULT(vkAllocateMemory(_owningDevice, &info, nullptr, &_resource));
		}
		virtual ~SafeVkDeviceMemory()
		{
			if (_resource)
			{
				vkFreeMemory(_owningDevice, _resource, nullptr);
				_resource = nullptr;
			}
			_owningDevice = nullptr;
		}
		VkDeviceMemory& Get()
		{
			return _resource;
		}
	};

	class SafeVkImageView : public SafeVkResource< VkImageView >
	{
	public:
		SafeVkImageView(GraphicsDevice* InOwner, const VkImageViewCreateInfo& info) :
			SafeVkResource< VkImageView >(InOwner)
		{
			SE_ASSERT(_resource == nullptr);
			VK_CHECK_RESULT(vkCreateImageView(_owningDevice, &info, nullptr, &_resource));
		}
		virtual ~SafeVkImageView()
		{
			if (_resource)
			{
				vkDestroyImageView(_owningDevice, _resource, nullptr);
				_resource = nullptr;
			}
			_owningDevice = nullptr;
		}
		VkImageView& Get()
		{
			return _resource;
		}
	};

	class SafeVkFrameBuffer : public SafeVkResource< VkFramebuffer >
	{
	public:
		SafeVkFrameBuffer(GraphicsDevice* InOwner, const VkFramebufferCreateInfo& info) :
			SafeVkResource< VkFramebuffer >(InOwner)
		{
			VK_CHECK_RESULT(vkCreateFramebuffer(_owningDevice, &info, nullptr, &_resource));
		}
		virtual ~SafeVkFrameBuffer()
		{
			if (_resource)
			{
				vkDestroyFramebuffer(_owningDevice, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkRenderPass : public SafeVkResource< VkRenderPass >
	{
	public:
		SafeVkRenderPass(GraphicsDevice* InOwner, const VkRenderPassCreateInfo& info) :
			SafeVkResource< VkRenderPass >(InOwner)
		{
			VK_CHECK_RESULT(vkCreateRenderPass(_owningDevice, &info, nullptr, &_resource));
		}
		virtual ~SafeVkRenderPass()
		{
			if (_resource)
			{
				vkDestroyRenderPass(_owningDevice, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkSampler : public SafeVkResource< VkSampler >
	{
	public:
		SafeVkSampler(GraphicsDevice* InOwner, const VkSamplerCreateInfo& info) :
			SafeVkResource< VkSampler >(InOwner)
		{
			VK_CHECK_RESULT(vkCreateSampler(_owningDevice, &info, nullptr, &_resource));
		}
		virtual ~SafeVkSampler()
		{
			if (_resource)
			{
				vkDestroySampler(_owningDevice, _resource, nullptr);
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
		SafeVkDescriptorSetLayout(GraphicsDevice* InOwner, const std::vector<VkDescriptorSetLayoutBinding>& bindings) :
			SafeVkResource< VkDescriptorSetLayout >(InOwner)
		{
			VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(bindings);
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(_owningDevice, &descriptorLayout, nullptr, &_resource));
		}

		virtual ~SafeVkDescriptorSetLayout()
		{
			if (_resource)
			{
				vkDestroyDescriptorSetLayout(_owningDevice, _resource, nullptr);
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
		SafeVkDescriptorSet(GraphicsDevice* InOwner, const VkDescriptorSetLayout& info, VkDescriptorPool InPool) :
			SafeVkResource< VkDescriptorSet >(InOwner), _usedPool(InPool)
		{
			VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(_usedPool, &info, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(_owningDevice, &allocInfo, &_resource));
		}

		virtual ~SafeVkDescriptorSet()
		{
			if (_resource)
			{
				vkFreeDescriptorSets(_owningDevice, _usedPool, 1, &_resource);
				_resource = nullptr;
			}
		}
	};
}   
