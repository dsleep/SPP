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

#include <ktx.h>
#include <ktxvulkan.h>

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

#if defined(__ANDROID__)
#	include <android/asset_manager.h>
#endif

namespace SPP
{
	ktxResult loadKTXFile(std::string filename, ktxTexture** target);

	class VulkanTextureBase
	{
	protected:
		//vks::VulkanDevice *   device;
		VkFormat			  _format = VK_FORMAT_UNDEFINED;
		VkImage               image;
		VkImageLayout         imageLayout;
		VkDeviceMemory        deviceMemory;
		VkImageView           view;
		uint32_t              width, height;
		uint32_t              mipLevels;
		uint32_t              layerCount;
		VkDescriptorImageInfo descriptor;
		VkSampler             sampler;

		void updateDescriptor();
		void destroy();	

	public:
		const VkDescriptorImageInfo& GetDescriptor()
		{
			return descriptor;
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

		void UpdateRect(int32_t rectX, int32_t rectY, int32_t Width, int32_t Height, const void* Data, uint32_t DataSize);
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
