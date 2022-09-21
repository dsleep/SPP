// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#pragma once

#include "VulkanDevice.h"


#include <fstream>
#include <stdlib.h>
#include <string>
#include <vector>

#include "vulkan/vulkan.h"

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

namespace SPP
{
	class VulkanTextureBase
	{
	protected:
		//vks::VulkanDevice *   device;
		VkFormat			  texformat = VK_FORMAT_UNDEFINED;
		VkImage               image = nullptr;
		VkImageLayout         _imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkDeviceMemory        deviceMemory = nullptr;
		VkImageView           view = nullptr;
		
		VkDescriptorImageInfo descriptor = {};
		VkSampler             sampler;
		uint32_t		      imageByteSize;

		void updateDescriptor();
		void destroy();	

	public:
		const VkDescriptorImageInfo& GetDescriptor()
		{
			return descriptor;
		}

		uint32_t GetImageSize() const
		{
			return imageByteSize;
		}

		const VkImage &GetVkImage() const
		{
			return image;
		}

		auto GetVkImageView() const
		{
			return view;
		}
	};

	class VulkanTexture : public VulkanTextureBase, public GPUTexture
	{
	protected:

		virtual void _MakeResident() override {}
		virtual void _MakeUnresident() override {}

	public:
		void loadFromFile(
			std::string        filename,
			VkFormat           format,
			vks::VulkanDevice* device,
			VkQueue            copyQueue,
			VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			bool               forceLinear = false);
		void fromBuffer(
			void* buffer,
			VkDeviceSize       bufferSize,
			VkFormat           format,
			uint32_t           texWidth,
			uint32_t           texHeight,
			vks::VulkanDevice* device,
			VkQueue            copyQueue,
			VkFilter           filter = VK_FILTER_LINEAR,
			VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout      imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		VulkanTexture(GraphicsDevice* InOwner, int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);
		VulkanTexture(GraphicsDevice* InOwner, int32_t Width, int32_t Height, TextureFormat Format);
		VulkanTexture(GraphicsDevice* InOwner, int32_t Width, int32_t Height, int32_t MipLevelCount, TextureFormat Format, VkImageUsageFlags UsageFlags);

		VulkanTexture(GraphicsDevice* InOwner, const struct TextureAsset& InTextureAsset);

		std::vector< GPUReferencer< SafeVkImageView > > GetMipChainViews();

		virtual void PushAsyncUpdate(Vector2i Start, Vector2i Extents, const void* Memory, uint32_t MemorySize) override;


		void UpdateRect(int32_t rectX, int32_t rectY, int32_t Width, int32_t Height, const void* Data, uint32_t DataSize);

		virtual void SetName(const char* InName) override;

		virtual ~VulkanTexture() 
		{ 
			destroy();
		}
	};

	//class Texture2DArray : public Texture
	//{
	//  public:
	//	void loadFromFile(
	//		std::string        filename,
	//		VkFormat           format,
	//		vks::VulkanDevice *device,
	//		VkQueue            copyQueue,
	//		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
	//		VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//};

	//class TextureCubeMap : public Texture
	//{
	//  public:
	//	void loadFromFile(
	//		std::string        filename,
	//		VkFormat           format,
	//		vks::VulkanDevice *device,
	//		VkQueue            copyQueue,
	//		VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
	//		VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//};
}
