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
#include "VulkanTexture.h"

namespace vks
{
	struct VulkanDevice;
}

namespace SPP
{
	struct VkFrameDataContainer
	{
		uint8_t ColorTargets = 0;
		uint8_t DepthStencilTargets = 0;

		GPUReferencer<SafeVkRenderPass> renderPass;
		GPUReferencer<SafeVkFrameBuffer> frameBuffer;

		uint32_t Width = 0;
		uint32_t Height = 0;

		std::vector<VkAttachmentDescription> attachmentDescriptions;
		bool bUseInvertedZ = true;

		std::array<VkClearValue, 5> clearValueArray;

		VkRenderPassBeginInfo SetupDrawPass(const Vector2i &InRenderArea)
		{
			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.pNext = nullptr;
			renderPassBeginInfo.renderPass = renderPass->Get();
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent.width = InRenderArea[0];
			renderPassBeginInfo.renderArea.extent.height = InRenderArea[1];			
			renderPassBeginInfo.framebuffer = frameBuffer->Get();
			auto totalClears = ColorTargets + DepthStencilTargets;

			for (auto& curClear : clearValueArray)
			{
				curClear.color = { 0.0f, 0.0f, 0.0f, 1.0f };
				curClear.depthStencil = { bUseInvertedZ ? 0 : 1.0f, 0 };
			}

			if (totalClears)
			{
				renderPassBeginInfo.pClearValues = clearValueArray.data();
			}
			else
			{
				renderPassBeginInfo.pClearValues = nullptr;
			}
			renderPassBeginInfo.clearValueCount = totalClears;
			return renderPassBeginInfo;
		}
	};
	/**
	* @brief Encapsulates a single frame buffer attachment 
	*/
	struct FramebufferAttachment
	{
		std::string name;
		GPUReferencer< class VulkanTexture > texture;
		VkAttachmentDescription description;
	};

	/**
	* @brief Encapsulates a complete Vulkan framebuffer with an arbitrary number and combination of attachments
	*/
	class VulkanFramebuffer
	{
	public:
		struct AttachmentCreateInfo
		{
			GPUReferencer< class VulkanTexture > texture;
			std::string name = "NOTSET";
			VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		};

		uint32_t _width = 0, _height = 0;

		std::list<FramebufferAttachment> attachments;

		/**
		* Default constructor
		*
		* @param vulkanDevice Pointer to a valid VulkanDevice
		*/
		VulkanFramebuffer(uint32_t InWidth, uint32_t InHeight);

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

		FramebufferAttachment& GetFrontAttachment()
		{
			return attachments.front();
		}

		FramebufferAttachment& GetBackAttachment()
		{
			return attachments.back();
		}

		//FramebufferAttachment* GetAttachment(const std::string &InName)
		//{
		//	for (auto & curAttach : attachments)
		//	{
		//		if (curAttach.name == InName)
		//		{
		//			return &curAttach;
		//		}
		//	}
		//	return nullptr;
		//}

		auto& GetAttachments()
		{
			return attachments;
		}

		VkFrameDataContainer createCustomRenderPass(const std::set<std::string> &WhichTargets, VkAttachmentLoadOp SetLoadOp);
		VkFrameDataContainer createCustomRenderPass(const std::map<std::string, VkAttachmentLoadOp>& TargetMap);

		VkDescriptorImageInfo GetImageInfo();
	};
}