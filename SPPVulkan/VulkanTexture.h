// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#pragma once

#include "VulkanDevice.h"
#include "vulkan/vulkan.h"

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

namespace SPP
{
	/// <summary>
	/// 
	/// </summary>
	class VulkanTexture : public GPUTexture
	{
	protected:
		VkFormat								_texformat = VK_FORMAT_UNDEFINED;
		VkImageLayout							_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout							_initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageUsageFlags						_usageFlags = 0;

		std::unique_ptr< SafeVkImage >          _image;
		std::unique_ptr< SafeVkDeviceMemory >   _deviceMemory;
		std::unique_ptr< SafeVkImageView >		_view;
		//TODO make this a global cached dealio like PSO
		std::unique_ptr< SafeVkSampler >        _sampler;

		VkImageSubresourceRange					_subresourceRange = {};
		VkDescriptorImageInfo					_descriptor = {};
		uint32_t								_imageByteSize = 0;

		void updateDescriptor();
		void destroy();

		virtual void _MakeResident() override {}
		virtual void _MakeUnresident() override {}

		void _allocate();

	public:

		VulkanTexture(int32_t Width, int32_t Height, TextureFormat Format);
		VulkanTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);
		VulkanTexture(int32_t Width, int32_t Height,
			int32_t MipLevelCount, int32_t FaceCount,
			TextureFormat Format, VkImageUsageFlags UsageFlags, VkImageLayout InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED );
		VulkanTexture(const struct TextureAsset& InTextureAsset);

		std::vector< GPUReferencer< SafeVkImageView > > GetMipChainViews();

		virtual void PushAsyncUpdate(Vector2i Start, Vector2i Extents, const void* Memory, uint32_t MemorySize) override;
		void UpdateRect(int32_t rectX, int32_t rectY, int32_t Width, int32_t Height, const void* Data, uint32_t DataSize);

		virtual void SetName(const char* InName) override;

		auto GetUsageFlags() { return _usageFlags; }
		bool hasDepth();
		bool hasStencil();
		bool isDepthStencil() { return(hasDepth() || hasStencil()); }

		const VkDescriptorImageInfo& GetDescriptor()
		{
			return _descriptor;
		}

		const auto& GetSubresourceRange()
		{
			return _subresourceRange;
		}

		VkImageAspectFlags GetImageAspect()
		{
			return _subresourceRange.aspectMask;
		}

		auto GetVkFormat()
		{
			return _texformat;
		}

		uint32_t GetImageSize() const
		{
			return _imageByteSize;
		}

		const VkImage& GetVkImage() const
		{
			return _image->Get();
		}

		auto GetVkImageView() const
		{
			return _view->Get();
		}

		virtual ~VulkanTexture() 
		{ 
			destroy();
		}
	};
}
