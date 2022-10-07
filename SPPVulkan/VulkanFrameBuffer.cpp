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
	VulkanFramebuffer::VulkanFramebuffer(GraphicsDevice* InOwner,
		uint32_t InWidth, uint32_t InHeight) : _owner(InOwner), _width(InWidth), _height(InHeight)
	{
	}

	/**
	* Destroy and free Vulkan resources used for the framebuffer and all of its attachments
	*/
	VulkanFramebuffer::~VulkanFramebuffer()
	{
		attachments.clear();
	}

	uint32_t VulkanFramebuffer::addAttachment(AttachmentCreateInfo createinfo)
	{
		FramebufferAttachment attachment;

		attachment.texture = createinfo.texture;
		attachment.name = createinfo.name;

		auto TextureRef = createinfo.texture.get();

#if 0
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
#endif

		// Fill attachment description
		attachment.description = {};
		attachment.description.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.description.storeOp = (TextureRef->GetUsageFlags() & VK_IMAGE_USAGE_SAMPLED_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.description.format = TextureRef->GetVkFormat();
		attachment.description.initialLayout = createinfo.initialLayout;
		// Final layout
		// If not, final layout depends on attachment type
		if (TextureRef->isDepthStencil())
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

	VkFrameDataContainer VulkanFramebuffer::createCustomRenderPass(const std::map<std::string, VkAttachmentLoadOp>& TargetMap)
	{
		VkFrameDataContainer oData;
				
		// Collect attachment references
		std::vector<VkAttachmentReference> colorReferences;
		VkAttachmentReference depthReference = {};

		bool hasDepth = false;
		bool hasStencil = false;
		bool hasColor = false;

		// Find. max number of layers across attachments
		uint32_t maxLayers = 0;
		std::vector<VkImageView> attachmentViews;

		uint32_t attachmentIndex = 0;
		for (auto& attachment : attachments)
		{
			auto foundLink = TargetMap.find(attachment.name);
			if (foundLink != TargetMap.end())
			{
				oData.attachmentDescriptions.push_back(attachment.description);
				oData.attachmentDescriptions.back().loadOp = foundLink->second;

				bool IsLoadLoadOp = (foundLink->second == VK_ATTACHMENT_LOAD_OP_LOAD);

				auto textureRef = attachment.texture.get();
				attachmentViews.push_back(textureRef->GetVkImageView());
				if (textureRef->GetSubresourceRange().layerCount > maxLayers)
				{
					maxLayers = textureRef->GetSubresourceRange().layerCount;
				}

				bool IsDepth = attachment.texture->hasDepth();
				bool IsStencil = attachment.texture->hasStencil();

				if (IsDepth || IsStencil)
				{
					//VkImageLayout textureLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					//if (IsLoadLoadOp)
					//{
					//	if (IsDepth && IsStencil)
					//	{
					//		textureLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
					//	}
					//	else if (IsDepth)
					//	{
					//		textureLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
					//	}
					//	else if (IsStencil)
					//	{
					//		textureLayout = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
					//	}
					//}
					//else
					//{
					//	if (IsDepth && IsStencil)
					//	{
					//		textureLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					//	}
					//	else if (IsDepth)
					//	{
					//		textureLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
					//	}
					//	else if (IsStencil)
					//	{
					//		textureLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
					//	}
					//}

					//depthReference.layout = textureLayout;

					depthReference.layout = IsLoadLoadOp ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
						VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

					// Only one depth attachment allowed
					//assert(!hasDepth);
					depthReference.attachment = attachmentIndex;
					hasDepth |= IsDepth;
					hasStencil |= IsStencil;

					oData.DepthStencilTargets++;
				}
				else
				{
					colorReferences.push_back({ attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
					hasColor = true;

					oData.ColorTargets++;
				}
				attachmentIndex++;
			}
		}

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
		renderPassInfo.pAttachments = oData.attachmentDescriptions.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(oData.attachmentDescriptions.size());
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();
		oData.renderPass = Make_GPU(SafeVkRenderPass, _owner, renderPassInfo);

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = oData.renderPass->Get();
		framebufferInfo.pAttachments = attachmentViews.data();
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
		framebufferInfo.width = _width;
		framebufferInfo.height = _height;
		framebufferInfo.layers = maxLayers;
		oData.frameBuffer = Make_GPU(SafeVkFrameBuffer, _owner, framebufferInfo);

		oData.Width = _width;
		oData.Height = _height;

		return oData;
	}

	VkFrameDataContainer VulkanFramebuffer::createCustomRenderPass(const std::set<std::string>& WhichTargets, VkAttachmentLoadOp SetLoadOp)
	{
		std::map<std::string, VkAttachmentLoadOp> TargetMap;
		for (auto& curTarget : WhichTargets)
		{
			TargetMap[curTarget] = SetLoadOp;
		}
		return createCustomRenderPass(TargetMap);
	}

	VkDescriptorImageInfo VulkanFramebuffer::GetImageInfo()
	{
		return attachments.front().texture->GetDescriptor();
	}
}