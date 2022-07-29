// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#pragma once


#include "VulkanFrameBuffer.hpp"
#include "VulkanDevice.h"

namespace SPP
{
	FramebufferAttachment::FramebufferAttachment(FramebufferAttachment&& moveBuffer)
	{
		image = std::move(moveBuffer.image);
		memory = std::move(moveBuffer.memory);
		view = std::move(moveBuffer.view);

		format = moveBuffer.format;
		subresourceRange = moveBuffer.subresourceRange;
		description = moveBuffer.description;
	}

	bool FramebufferAttachment::hasDepth()
	{
		std::vector<VkFormat> formats =
		{
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
		};
		return std::find(formats.begin(), formats.end(), format) != std::end(formats);
	}

	bool FramebufferAttachment::hasStencil()
	{
		std::vector<VkFormat> formats =
		{
			VK_FORMAT_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
		};
		return std::find(formats.begin(), formats.end(), format) != std::end(formats);
	}

	/**
	* @brief Returns true if the attachment is a depth and/or stencil attachment
	*/
	bool FramebufferAttachment::isDepthStencil()
	{
		return(hasDepth() || hasStencil());
	}

	VulkanFramebuffer::VulkanFramebuffer(GraphicsDevice* InOwner, vks::VulkanDevice* vulkanDevice,
		uint32_t InWidth, uint32_t InHeight) : _owner(InOwner), width(InWidth), height(InHeight)
	{
		assert(vulkanDevice);
		this->vulkanDevice = vulkanDevice;
	}

	/**
	* Destroy and free Vulkan resources used for the framebuffer and all of its attachments
	*/
	VulkanFramebuffer::~VulkanFramebuffer()
	{
		assert(vulkanDevice);
		for (auto& attachment : attachments)
		{
			attachment.image.Reset();
			attachment.view.Reset();
			attachment.memory.Reset();
		}
		sampler.Reset();
		renderPass.Reset();
		framebuffer.Reset();
	}

	uint32_t VulkanFramebuffer::addAttachment(AttachmentCreateInfo createinfo)
	{
		FramebufferAttachment attachment;

		attachment.format = createinfo.format;

		VkImageAspectFlags aspectMask = VK_FLAGS_NONE;

		// Select aspect mask and layout depending on usage

		// Color attachment
		if (createinfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		// Depth (and/or stencil) attachment
		if (createinfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			if (attachment.hasDepth())
			{
				aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			if (attachment.hasStencil())
			{
				aspectMask = aspectMask | VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}

		assert(aspectMask > 0);

		VkImageCreateInfo image = vks::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = createinfo.format;
		image.extent.width = createinfo.width;
		image.extent.height = createinfo.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = createinfo.layerCount;
		image.samples = createinfo.imageSampleCount;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = createinfo.usage;

		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Create image for this attachment
		attachment.image = Make_GPU(SafeVkImage, _owner, image);
		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, attachment.image->Get(), &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		attachment.memory = Make_GPU(SafeVkDeviceMemory, _owner, memAlloc);
		VK_CHECK_RESULT(vkBindImageMemory(vulkanDevice->logicalDevice, attachment.image->Get(), attachment.memory->Get(), 0));

		attachment.subresourceRange = {};
		attachment.subresourceRange.aspectMask = aspectMask;
		attachment.subresourceRange.levelCount = 1;
		attachment.subresourceRange.layerCount = createinfo.layerCount;

		VkImageViewCreateInfo imageView = vks::initializers::imageViewCreateInfo();
		imageView.viewType = (createinfo.layerCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		imageView.format = createinfo.format;
		imageView.subresourceRange = attachment.subresourceRange;
		//todo: workaround for depth+stencil attachments
		imageView.subresourceRange.aspectMask = (attachment.hasDepth()) ? VK_IMAGE_ASPECT_DEPTH_BIT : aspectMask;
		imageView.image = attachment.image->Get();

		attachment.view = Make_GPU(SafeVkImageView, _owner, imageView);

		// Fill attachment description
		attachment.description = {};
		attachment.description.samples = createinfo.imageSampleCount;
		attachment.description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.description.storeOp = (createinfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.description.format = createinfo.format;
		attachment.description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		// Final layout
		// If not, final layout depends on attachment type
		if (attachment.hasDepth() || attachment.hasStencil())
		{
			attachment.description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else
		{
			attachment.description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		attachments.push_back(std::move(attachment));

		return static_cast<uint32_t>(attachments.size() - 1);
	}


	void VulkanFramebuffer::createSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode adressMode)
	{
		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = magFilter;
		samplerInfo.minFilter = minFilter;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = adressMode;
		samplerInfo.addressModeV = adressMode;
		samplerInfo.addressModeW = adressMode;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		sampler = Make_GPU(SafeVkSampler, _owner, samplerInfo);
	}

	VkFrameData VulkanFramebuffer::GetFrameData()
	{
		return VkFrameData{ .renderPass = renderPass->Get(), .frameBuffer = framebuffer->Get() };
	}

	VkDescriptorImageInfo VulkanFramebuffer::GetImageInfo()
	{
		return VkDescriptorImageInfo{ 
			.sampler = sampler->Get(),
			.imageView = attachments.front().view->Get(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	}

	VkDescriptorImageInfo VulkanFramebuffer::GetBackImageInfo()
	{
		return VkDescriptorImageInfo{
			.sampler = sampler->Get(),
			.imageView = attachments.back().view->Get(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	}

	VkResult VulkanFramebuffer::createRenderPass()
	{
		std::vector<VkAttachmentDescription> attachmentDescriptions;
		for (auto& attachment : attachments)
		{
			attachmentDescriptions.push_back(attachment.description);
		};

		// Collect attachment references
		std::vector<VkAttachmentReference> colorReferences;
		VkAttachmentReference depthReference = {};
		bool hasDepth = false;
		bool hasColor = false;

		uint32_t attachmentIndex = 0;

		for (auto& attachment : attachments)
		{
			if (attachment.isDepthStencil())
			{
				// Only one depth attachment allowed
				assert(!hasDepth);
				depthReference.attachment = attachmentIndex;
				depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				hasDepth = true;
			}
			else
			{
				colorReferences.push_back({ attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
				hasColor = true;
			}
			attachmentIndex++;
		};

		// Default render pass setup uses only one subpass
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		if (hasColor)
		{
			subpass.pColorAttachments = colorReferences.data();
			subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
		}
		if (hasDepth)
		{
			subpass.pDepthStencilAttachment = &depthReference;
		}

		// Use subpass dependencies for attachment layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create render pass
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pAttachments = attachmentDescriptions.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();
		renderPass = Make_GPU(SafeVkRenderPass, _owner, renderPassInfo);

		std::vector<VkImageView> attachmentViews;
		for (auto& attachment : attachments)
		{
			attachmentViews.push_back(attachment.view->Get());
		}

		// Find. max number of layers across attachments
		uint32_t maxLayers = 0;
		for (auto& attachment : attachments)
		{
			if (attachment.subresourceRange.layerCount > maxLayers)
			{
				maxLayers = attachment.subresourceRange.layerCount;
			}
		}

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass->Get();
		framebufferInfo.pAttachments = attachmentViews.data();
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = maxLayers;
		framebuffer = Make_GPU(SafeVkFrameBuffer, _owner, framebufferInfo);

		return VK_SUCCESS;
	}
}