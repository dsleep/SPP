// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#pragma once

#include <algorithm>
#include <iterator>
#include <vector>
#include "vulkan/vulkan.h"
#include "VulkanTools.h"
#include "VulkanResources.h"

namespace vks
{
	struct VulkanDevice;
}

namespace SPP
{
	/**
	* @brief Encapsulates a single frame buffer attachment 
	*/
	struct FramebufferAttachment
	{
		NO_COPY_ALLOWED(FramebufferAttachment)

		std::unique_ptr<SafeVkImage> image;
		std::unique_ptr<SafeVkDeviceMemory> memory;
		std::unique_ptr<SafeVkImageView> view;

		VkFormat format;
		VkImageSubresourceRange subresourceRange;
		VkAttachmentDescription description;

		FramebufferAttachment() = default;
		FramebufferAttachment(FramebufferAttachment&& moveBuffer);
		/**
		* @brief Returns true if the attachment has a depth component
		*/
		bool hasDepth();

		/**
		* @brief Returns true if the attachment has a stencil component
		*/
		bool hasStencil();

		/**
		* @brief Returns true if the attachment is a depth and/or stencil attachment
		*/
		bool isDepthStencil();
	};

	/**
	* @brief Describes the attributes of an attachment to be created
	*/
	struct AttachmentCreateInfo
	{
		uint32_t width, height;
		uint32_t layerCount;
		VkFormat format;
		VkImageUsageFlags usage;
		VkSampleCountFlagBits imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
	};

	/**
	* @brief Encapsulates a complete Vulkan framebuffer with an arbitrary number and combination of attachments
	*/
	class VulkanFramebuffer
	{
	private:
		vks::VulkanDevice *vulkanDevice;

	public:
		uint32_t width, height;

		std::unique_ptr<SafeVkFrameBuffer> framebuffer;
		std::list<FramebufferAttachment> attachments;

		std::unique_ptr<SafeVkRenderPass>  renderPass;
		std::unique_ptr<SafeVkSampler> sampler;

		/**
		* Default constructor
		*
		* @param vulkanDevice Pointer to a valid VulkanDevice
		*/
		VulkanFramebuffer(vks::VulkanDevice* vulkanDevice);

		/**
		* Destroy and free Vulkan resources used for the framebuffer and all of its attachments
		*/
		~VulkanFramebuffer();

		/**
		* Add a new attachment described by createinfo to the framebuffer's attachment list
		*
		* @param createinfo Structure that specifies the framebuffer to be constructed
		*
		* @return Index of the new attachment
		*/
		uint32_t addAttachment(AttachmentCreateInfo createinfo);

		/**
		* Creates a default sampler for sampling from any of the framebuffer attachments
		* Applications are free to create their own samplers for different use cases 
		*
		* @param magFilter Magnification filter for lookups
		* @param minFilter Minification filter for lookups
		* @param adressMode Addressing mode for the U,V and W coordinates
		*/
		void createSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode adressMode);

		/**
		* Creates a default render pass setup with one sub pass
		*
		* @return VK_SUCCESS if all resources have been created successfully
		*/
		VkResult createRenderPass();
	};
}