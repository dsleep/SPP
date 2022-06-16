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
	class SafeVkCommandBuffer
	{
	private:
		VkDevice _owningDevice = nullptr;
		VkCommandBuffer _cmdBuf = nullptr;
		VkCommandPool _owningPool = nullptr;

	public:
		SafeVkCommandBuffer(VkDevice InDevice, const VkCommandBufferAllocateInfo& info)
		{
			SE_ASSERT(_cmdBuf == nullptr);
			_owningDevice = InDevice;
			_owningPool = info.commandPool;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(_owningDevice, &info, &_cmdBuf));
		}
		~SafeVkCommandBuffer()
		{
			if (_cmdBuf)
			{
				vkFreeCommandBuffers(_owningDevice, _owningPool, 1, &_cmdBuf);
			}
		}
		VkCommandBuffer &Get()
		{
			return _cmdBuf;
		}
	};

	template<typename ResourceType>
	class SafeVkResource
	{
	protected:
		VkDevice _owningDevice = nullptr;
		ResourceType _resource = nullptr;

	public:
		SafeVkResource(VkDevice InDevice) : _owningDevice(InDevice)
		{
			SE_ASSERT(InDevice);
		}
		~SafeVkResource() { _owningDevice = nullptr; }
		ResourceType& Get()
		{
			return _resource;
		}
	};

	class SafeVkFence : public SafeVkResource< VkFence >
	{	
	public:
		SafeVkFence(VkDevice InDevice, const VkFenceCreateInfo &info) :
			SafeVkResource< VkFence >(InDevice)
		{
			VK_CHECK_RESULT(vkCreateFence(_owningDevice, &info, nullptr, &_resource));
		}
		~SafeVkFence()
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
		SafeVkImage(VkDevice InDevice, const VkImageCreateInfo& info) :
			SafeVkResource< VkImage >(InDevice)
		{
			VK_CHECK_RESULT(vkCreateImage(_owningDevice, &info, nullptr, &_resource));
		}
		~SafeVkImage()
		{
			if (_resource)
			{
				vkDestroyImage(_owningDevice, _resource, nullptr);
				_resource = nullptr;
			}
		}
	};

	class SafeVkDeviceMemory
	{
	private:
		VkDevice _owningDevice = nullptr;
		VkDeviceMemory _resource = nullptr;

	public:
		SafeVkDeviceMemory(VkDevice InDevice, const VkMemoryAllocateInfo& info)
		{
			SE_ASSERT(_resource == nullptr);
			_owningDevice = InDevice;
			VK_CHECK_RESULT(vkAllocateMemory(_owningDevice, &info, nullptr, &_resource));
		}
		~SafeVkDeviceMemory()
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

	class SafeVkImageView
	{
	private:
		VkDevice _owningDevice = nullptr;
		VkImageView _resource = nullptr;

	public:
		SafeVkImageView(VkDevice InDevice, const VkImageViewCreateInfo& info)
		{
			SE_ASSERT(_resource == nullptr);
			_owningDevice = InDevice;
			VK_CHECK_RESULT(vkCreateImageView(_owningDevice, &info, nullptr, &_resource));
		}
		~SafeVkImageView()
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
		SafeVkFrameBuffer(VkDevice InDevice, const VkFramebufferCreateInfo& info) : 
			SafeVkResource< VkFramebuffer >(InDevice)
		{
			VK_CHECK_RESULT(vkCreateFramebuffer(_owningDevice, &info, nullptr, &_resource));
		}
		~SafeVkFrameBuffer()
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
		SafeVkRenderPass(VkDevice InDevice, const VkRenderPassCreateInfo& info) :
			SafeVkResource< VkRenderPass >(InDevice)
		{
			VK_CHECK_RESULT(vkCreateRenderPass(_owningDevice, &info, nullptr, &_resource));
		}
		~SafeVkRenderPass()
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
		SafeVkSampler(VkDevice InDevice, const VkSamplerCreateInfo& info) :
			SafeVkResource< VkSampler >(InDevice)
		{
			VK_CHECK_RESULT(vkCreateSampler(_owningDevice, &info, nullptr, &_resource));
		}
		~SafeVkSampler()
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
		std::unique_ptr<SafeVkFence> fence;
		std::unique_ptr<SafeVkCommandBuffer> cmdBuf;
		bool bHasBegun = false;
	};	
}   
