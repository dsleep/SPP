// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#include <VulkanTexture.h>
#include "VulkanResources.h"
#include "VulkanDebug.h"

#include "SPPTextures.h"

//#include <ktx.h>
//#include <ktxvulkan.h>

namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	void VulkanTextureBase::updateDescriptor()
	{
		descriptor.sampler = sampler;
		descriptor.imageView = view;
		descriptor.imageLayout = imageLayout;
	}

	void VulkanTextureBase::destroy()
	{
		vkDestroyImageView(GGlobalVulkanDevice, view, nullptr);
		vkDestroyImage(GGlobalVulkanDevice, image, nullptr);
		if (sampler)
		{
			vkDestroySampler(GGlobalVulkanDevice, sampler, nullptr);
		}
		vkFreeMemory(GGlobalVulkanDevice, deviceMemory, nullptr);
	}

//	bool LoadKTX2FromMemory(const void *InData, size_t InDataSize)
//	{
//		ktxTexture2* texture2 = 0;
//		KTX_error_code result;
//
//		result = ktxTexture2_CreateFromMemory((const ktx_uint8_t*)InData, InDataSize,
//			KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
//			&texture2);
//
//		if (result != KTX_SUCCESS)return false;
//		if (texture2->classId != ktxTexture2_c)return false;
//
//		auto vkFormat = texture2->vkFormat;
//		ktx_uint32_t components = ktxTexture2_GetNumComponents(texture2);
//
//		auto width = texture2->baseWidth;
//		auto height = texture2->baseHeight;
//		auto mipLevels = texture2->numLevels;
//
//		for (ktx_uint32_t level = 0; level < texture2->numLevels; level++) 
//		{
//			ktx_size_t levelOffset;
//			result = ktxTexture_GetImageOffset(ktxTexture(texture2), level, 0, 0, &levelOffset);
//
//			SE_ASSERT(result == KTX_SUCCESS);
//		}
//
//		ktxTexture_Destroy(ktxTexture(texture2));
//
//		return true;
//	}
//
//	ktxResult loadKTXFile(std::string filename, ktxTexture **target)
//	{
//		ktxResult result = KTX_SUCCESS;
//#if defined(__ANDROID__)
//		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
//		if (!asset) {
//			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
//		}
//		size_t size = AAsset_getLength(asset);
//		assert(size > 0);
//		ktx_uint8_t *textureData = new ktx_uint8_t[size];
//		AAsset_read(asset, textureData, size);
//		AAsset_close(asset);
//		result = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);
//		delete[] textureData;
//#else
//		if (!vks::tools::fileExists(filename)) {
//			vks::tools::exitFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
//		}
//		result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);			
//#endif		
//		return result;
//	}

	/**
	* Load a 2D texture including all mip levels
	*
	* @param filename File to load (supports .ktx)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	* @param (Optional) forceLinear Force linear tiling (not advised, defaults to false)
	*
	*/
	void VulkanTexture::loadFromFile(std::string filename, 
		VkFormat format,
		vks::VulkanDevice *device, 
		VkQueue copyQueue, 
		VkImageUsageFlags imageUsageFlags, 
		VkImageLayout imageLayout, 
		bool forceLinear)
	{
		//ktxTexture* ktxTexture;
		//ktxResult result = loadKTXFile(filename, &ktxTexture);
		//assert(result == KTX_SUCCESS);


		//auto sfileName = stdfs::path(filename).filename().generic_string();

		////this->device = device;
		//width = ktxTexture->baseWidth;
		//height = ktxTexture->baseHeight;
		//mipLevels = ktxTexture->numLevels;

		//ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		//ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

		//// Get device properties for the requested texture format
		//VkFormatProperties formatProperties;
		//vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);

		//// Only use linear tiling if requested (and supported by the device)
		//// Support for linear tiling is mostly limited, so prefer to use
		//// optimal tiling instead
		//// On most implementations linear tiling will only support a very
		//// limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
		//VkBool32 useStaging = !forceLinear;

		//VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		//VkMemoryRequirements memReqs;

		//// Use a separate command buffer for texture loading
		//VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		//if (useStaging)
		//{
		//	// Create a host-visible staging buffer that contains the raw image data
		//	VkBuffer stagingBuffer;
		//	VkDeviceMemory stagingMemory;

		//	VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		//	bufferCreateInfo.size = ktxTextureSize;
		//	// This buffer is used as a transfer source for the buffer copy
		//	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		//	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		//	VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		//	// Get memory requirements for the staging buffer (alignment, memory type bits)
		//	vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

		//	memAllocInfo.allocationSize = memReqs.size;
		//	// Get memory type index for a host visible buffer
		//	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		//	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
		//	VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

		//	// Copy texture data into staging buffer
		//	uint8_t *data;
		//	VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		//	memcpy(data, ktxTextureData, ktxTextureSize);
		//	vkUnmapMemory(device->logicalDevice, stagingMemory);

		//	// Setup buffer copy regions for each mip level
		//	std::vector<VkBufferImageCopy> bufferCopyRegions;

		//	for (uint32_t i = 0; i < mipLevels; i++)
		//	{
		//		ktx_size_t offset;
		//		KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
		//		assert(result == KTX_SUCCESS);

		//		VkBufferImageCopy bufferCopyRegion = {};
		//		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		//		bufferCopyRegion.imageSubresource.mipLevel = i;
		//		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		//		bufferCopyRegion.imageSubresource.layerCount = 1;
		//		bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> i);
		//		bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> i);
		//		bufferCopyRegion.imageExtent.depth = 1;
		//		bufferCopyRegion.bufferOffset = offset;

		//		bufferCopyRegions.push_back(bufferCopyRegion);
		//	}

		//	// Create optimal tiled target image
		//	VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		//	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		//	imageCreateInfo.format = format;
		//	imageCreateInfo.mipLevels = mipLevels;
		//	imageCreateInfo.arrayLayers = 1;
		//	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		//	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		//	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//	imageCreateInfo.extent = { width, height, 1 };
		//	imageCreateInfo.usage = imageUsageFlags;
		//	// Ensure that the TRANSFER_DST bit is set for staging
		//	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		//	{
		//		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		//	}
		//	VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

		//	vks::debugmarker::setImageName(device->logicalDevice, image, sfileName.c_str());

		//	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

		//	memAllocInfo.allocationSize = memReqs.size;
		//	imageByteSize = memReqs.size;

		//	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
		//	VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

		//	VkImageSubresourceRange subresourceRange = {};
		//	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		//	subresourceRange.baseMipLevel = 0;
		//	subresourceRange.levelCount = mipLevels;
		//	subresourceRange.layerCount = 1;

		//	// Image barrier for optimal image (target)
		//	// Optimal image will be used as destination for the copy
		//	vks::tools::setImageLayout(
		//		copyCmd,
		//		image,
		//		VK_IMAGE_LAYOUT_UNDEFINED,
		//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		//		subresourceRange);

		//	// Copy mip levels from staging buffer
		//	vkCmdCopyBufferToImage(
		//		copyCmd,
		//		stagingBuffer,
		//		image,
		//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		//		static_cast<uint32_t>(bufferCopyRegions.size()),
		//		bufferCopyRegions.data()
		//	);

		//	// Change texture image layout to shader read after all mip levels have been copied
		//	this->imageLayout = imageLayout;
		//	vks::tools::setImageLayout(
		//		copyCmd,
		//		image,
		//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		//		imageLayout,
		//		subresourceRange);

		//	device->flushCommandBuffer(copyCmd, copyQueue);

		//	// Clean up staging resources
		//	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
		//	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
		//}
		//else
		//{
		//	// Prefer using optimal tiling, as linear tiling 
		//	// may support only a small set of features 
		//	// depending on implementation (e.g. no mip maps, only one layer, etc.)

		//	// Check if this support is supported for linear tiling
		//	assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

		//	VkImage mappableImage;
		//	VkDeviceMemory mappableMemory;

		//	VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		//	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		//	imageCreateInfo.format = format;
		//	imageCreateInfo.extent = { width, height, 1 };
		//	imageCreateInfo.mipLevels = 1;
		//	imageCreateInfo.arrayLayers = 1;
		//	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		//	imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
		//	imageCreateInfo.usage = imageUsageFlags;
		//	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		//	// Load mip map level 0 to linear tiling image
		//	VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &mappableImage));


		//	vks::debugmarker::setImageName(device->logicalDevice, image, sfileName.c_str());

		//	// Get memory requirements for this image 
		//	// like size and alignment
		//	vkGetImageMemoryRequirements(device->logicalDevice, mappableImage, &memReqs);
		//	// Set memory allocation size to required memory size
		//	memAllocInfo.allocationSize = memReqs.size;
		//	imageByteSize = memReqs.size;
		//	// Get memory type that can be mapped to host memory
		//	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		//	// Allocate host memory
		//	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &mappableMemory));

		//	// Bind allocated image for use
		//	VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, mappableImage, mappableMemory, 0));

		//	// Get sub resource layout
		//	// Mip map count, array layer, etc.
		//	VkImageSubresource subRes = {};
		//	subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		//	subRes.mipLevel = 0;

		//	VkSubresourceLayout subResLayout;
		//	void *data;

		//	// Get sub resources layout 
		//	// Includes row pitch, size offsets, etc.
		//	vkGetImageSubresourceLayout(device->logicalDevice, mappableImage, &subRes, &subResLayout);

		//	// Map image memory
		//	VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, mappableMemory, 0, memReqs.size, 0, &data));

		//	// Copy image data into memory
		//	memcpy(data, ktxTextureData, memReqs.size);

		//	vkUnmapMemory(device->logicalDevice, mappableMemory);

		//	// Linear tiled images don't need to be staged
		//	// and can be directly used as textures
		//	image = mappableImage;
		//	deviceMemory = mappableMemory;
		//	this->imageLayout = imageLayout;

		//	// Setup image memory barrier
		//	vks::tools::setImageLayout(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, imageLayout);

		//	device->flushCommandBuffer(copyCmd, copyQueue);
		//}

		//ktxTexture_Destroy(ktxTexture);

		//// Create a default sampler
		//VkSamplerCreateInfo samplerCreateInfo = {};
		//samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		//samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		//samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		//samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		//samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//samplerCreateInfo.mipLodBias = 0.0f;
		//samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		//samplerCreateInfo.minLod = 0.0f;
		//// Max level-of-detail should match mip level count
		//samplerCreateInfo.maxLod = (useStaging) ? (float)mipLevels : 0.0f;
		//// Only enable anisotropic filtering if enabled on the device
		//samplerCreateInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
		//samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
		//samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		//VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

		//// Create image view
		//// Textures are not directly accessed by the shaders and
		//// are abstracted by image views containing additional
		//// information and sub resource ranges
		//VkImageViewCreateInfo viewCreateInfo = {};
		//viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		//viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		//viewCreateInfo.format = format;
		//viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		//viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		//// Linear tiling usually won't support mip maps
		//// Only set mip map count if optimal tiling is used
		//viewCreateInfo.subresourceRange.levelCount = (useStaging) ? mipLevels : 1;
		//viewCreateInfo.image = image;
		//VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

		//// Update descriptor image info member that can be used for setting up descriptor sets
		//updateDescriptor();
	}

	/**
	* Creates a 2D texture from a buffer
	*
	* @param buffer Buffer containing texture data to upload
	* @param bufferSize Size of the buffer in machine units
	* @param width Width of the texture to create
	* @param height Height of the texture to create
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) filter Texture filtering for the sampler (defaults to VK_FILTER_LINEAR)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	*/
	void VulkanTexture::fromBuffer(void* buffer, 
		VkDeviceSize bufferSize, 
		VkFormat format,
		uint32_t texWidth, 
		uint32_t texHeight, 
		vks::VulkanDevice *device, 
		VkQueue copyQueue,
		VkFilter filter, 
		VkImageUsageFlags imageUsageFlags, 
		VkImageLayout imageLayout)
	{
		assert(buffer);

		//this->device = device;
		width = texWidth;
		height = texHeight;
		mipLevels = 1;

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Use a separate command buffer for texture loading
		VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = bufferSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, buffer, bufferSize);
		vkUnmapMemory(device->logicalDevice, stagingMemory);

		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = width;
		bufferCopyRegion.imageExtent.height = height;
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = 0;

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = mipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { width, height, 1 };
		imageCreateInfo.usage = imageUsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		{
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

		vks::debugmarker::setImageName(device->logicalDevice, image, "VT_fromBuffer");

		vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		imageByteSize = memReqs.size;

		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.layerCount = 1;

		// Image barrier for optimal image (target)
		// Optimal image will be used as destination for the copy
		vks::tools::setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy mip levels from staging buffer
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion
		);

		// Change texture image layout to shader read after all mip levels have been copied
		this->imageLayout = imageLayout;
		vks::tools::setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			imageLayout,
			subresourceRange);

		device->flushCommandBuffer(copyCmd, copyQueue);

		// Clean up staging resources
		vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
		vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

		// Create sampler
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = filter;
		samplerCreateInfo.minFilter = filter;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

		// Create image view
		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.pNext = NULL;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = format;
		viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.image = image;
		VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}

	VulkanTexture::VulkanTexture(GraphicsDevice* InOwner, int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo)
		: GPUTexture(InOwner, Width, Height, Format, RawData, InMetaInfo)
	{
		fromBuffer(RawData->GetElementData(), RawData->GetTotalSize(), VK_FORMAT_R8G8B8A8_UNORM,
			Width, Height, GGlobalVulkanGI->GetVKSVulkanDevice(), GGlobalVulkanGI->GetGraphicsQueue());
	}

	void VulkanTexture::SetName(const char* InName)
	{
		vks::debugmarker::setImageName(GGlobalVulkanDevice, image, InName);
	}

	VkFormat SPPToVulkan(TextureFormat InFormat)
	{
		switch (InFormat)
		{
		case TextureFormat::RGBA_8888:
			return VK_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat::BGRA_8888:
			return VK_FORMAT_B8G8R8A8_UNORM;

			
		case TextureFormat::R32F:
			return VK_FORMAT_R32_SFLOAT;
		}

		SE_ASSERT(false);
		return VK_FORMAT_UNDEFINED;
	}

	VkFormat TextureFormatToVKFormat(TextureFormat InFormat, bool isSRGB)
	{
		switch (InFormat)
		{
		case TextureFormat::RGB_888:
			return isSRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
			break;
		case TextureFormat::RGBA_8888:
			return isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
			break;
		case TextureFormat::RGB_BC1:
			return isSRGB ? VK_FORMAT_BC1_RGB_SRGB_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
			break;
		case TextureFormat::RGBA_BC7:
			return isSRGB ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;
			break;


		case TextureFormat::BGRA_8888:
		case TextureFormat::RG_BC5:
		case TextureFormat::GRAY_BC4:
		case TextureFormat::D24_S8:
		case TextureFormat::R32F:
		case TextureFormat::R32G32B32A32F:
		case TextureFormat::R32G32B32A32:
			//TODO lazyness
			SE_ASSERT(false);
			break;
		}

		return VK_FORMAT_UNDEFINED;
	}

	VulkanTexture::VulkanTexture(GraphicsDevice* InOwner, const struct TextureAsset& InTextureAsset) : GPUTexture(InOwner, InTextureAsset)
	{
		auto sfileName = stdfs::path(InTextureAsset.orgFileName).filename().generic_string();

		texformat = TextureFormatToVKFormat(InTextureAsset.format, InTextureAsset.bSRGB);
		auto TotalTextureSize = InTextureAsset.GetTotalSize();
		auto copyQueue = GGlobalVulkanGI->GetGraphicsQueue();
		auto device = GGlobalVulkanGI->GetVKSVulkanDevice();

		width = InTextureAsset.width;
		height = InTextureAsset.height;
		mipLevels = InTextureAsset.mipData.size();
		layerCount = 1;

		// Get device properties for the requested texture format
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(device->physicalDevice, texformat, &formatProperties);

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Use a separate command buffer for texture loading
		VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = TotalTextureSize;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));
				
		// Copy texture data into staging buffer
		uint8_t* data;
		VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));

		// Setup buffer copy regions for each mip level
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		size_t currentOffset = 0;
		uint32_t currentIdx = 0;
		for (auto& curMip : InTextureAsset.mipData)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = currentIdx;
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = std::max< int32_t>(1u, _width >> currentIdx);
			bufferCopyRegion.imageExtent.height = std::max< int32_t>(1u, _height >> currentIdx);
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = currentOffset;

			memcpy(data + currentOffset, curMip->GetElementData(), curMip->GetTotalSize());
			currentOffset += curMip->GetTotalSize();
			currentIdx++;

			bufferCopyRegions.push_back(bufferCopyRegion);
		}
		
		vkUnmapMemory(device->logicalDevice, stagingMemory);

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = texformat;
		imageCreateInfo.mipLevels = mipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { width, height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

		vks::debugmarker::setImageName(device->logicalDevice, image, sfileName.c_str());

		vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		imageByteSize = memReqs.size;

		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.layerCount = 1;

		// Image barrier for optimal image (target)
		// Optimal image will be used as destination for the copy
		vks::tools::setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Copy mip levels from staging buffer
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);

		// Change texture image layout to shader read after all mip levels have been copied
		this->imageLayout = imageLayout;
		vks::tools::setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			imageLayout,
			subresourceRange);

		device->flushCommandBuffer(copyCmd, copyQueue);

		// Clean up staging resources
		vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
		vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);


		// Create a default sampler
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		// Max level-of-detail should match mip level count
		samplerCreateInfo.maxLod = (float)mipLevels;
		// Only enable anisotropic filtering if enabled on the device
		samplerCreateInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
		samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

		// Create image view
		// Textures are not directly accessed by the shaders and
		// are abstracted by image views containing additional
		// information and sub resource ranges
		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = texformat;
		viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 };		
		viewCreateInfo.image = image;
		VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}

	VulkanTexture::VulkanTexture(GraphicsDevice* InOwner, int32_t Width, int32_t Height, int32_t MipLevelCount, TextureFormat Format, VkImageUsageFlags UsageFlags)
		: GPUTexture(InOwner, Width, Height, Format, nullptr, nullptr)
	{
		width = Width;
		height = Height;
		mipLevels = MipLevelCount;
		layerCount = 1;

		auto copyQueue = GGlobalVulkanGI->GetGraphicsQueue();
		auto device = GGlobalVulkanGI->GetVKSVulkanDevice();
		VkFormat format = SPPToVulkan(Format);
		VkFilter filter = VK_FILTER_NEAREST;

		texformat = format;

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = mipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { width, height, 1 };
		imageCreateInfo.usage = UsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		{
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

		vks::debugmarker::setImageName(device->logicalDevice, image, "VT_Empty");

		VkMemoryRequirements memReqs = { 0 };
		vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

		// Use a separate command buffer for texture loading
		VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 };

		vks::tools::setImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);

		device->flushCommandBuffer(copyCmd, copyQueue);

		this->imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageByteSize = memReqs.size;

		// Create sampler
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = filter;
		samplerCreateInfo.minFilter = filter;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 0.0f;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

		// Create image view
		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.pNext = NULL;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = format;
		viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 };
		viewCreateInfo.image = image;
		VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

		// Update descriptor image info member that can be used for setting up descriptor sets
		updateDescriptor();
	}

	std::vector< GPUReferencer< SafeVkImageView > > VulkanTexture::GetMipChainViews() 
	{
		//VkImageAspectFlags aspectMask = (format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		std::vector< GPUReferencer< SafeVkImageView > > oViews;
		for (int32_t Iter = 0; Iter < mipLevels; Iter++)
		{
			VkImageViewCreateInfo viewCreateInfo = {};
			viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewCreateInfo.pNext = NULL;
			viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewCreateInfo.format = texformat;
			viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)Iter, 1, 0, 1 };			
			viewCreateInfo.image = image;

			auto newView = Make_GPU(SafeVkImageView, GGlobalVulkanGI, viewCreateInfo);
			oViews.push_back(newView);
		}

		return oViews;
	}

	VulkanTexture::VulkanTexture(GraphicsDevice* InOwner, int32_t Width, int32_t Height, TextureFormat Format) : 
		VulkanTexture(InOwner, Width, Height, 1, Format, VK_IMAGE_USAGE_SAMPLED_BIT |
			VK_IMAGE_USAGE_STORAGE_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	{
	}

	void VulkanTexture::UpdateRect(int32_t rectX, int32_t rectY, int32_t Width, int32_t Height, const void* Data, uint32_t DataSize)
	{
		
	}

	void VulkanTexture::PushAsyncUpdate(Vector2i Start, Vector2i Extents, const void* Data, uint32_t DataSize)
	{
		SE_ASSERT(IsOnGPUThread());

		//HACK just assumes RGBA8888
		SE_ASSERT((Extents[0] * Extents[1] * 4) == DataSize);
		auto& perFrameScratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		auto& cmdBuffer = GGlobalVulkanGI->GetCopyCommandBuffer();
		auto activeFrame = GGlobalVulkanGI->GetActiveFrame();

		auto WritableChunk = perFrameScratchBuffer.Write((const uint8_t*)Data, DataSize, activeFrame);

		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = Extents[0];
		bufferCopyRegion.imageExtent.height = Extents[1];
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = WritableChunk.offsetFromBase;
		bufferCopyRegion.imageOffset.x = Start[0];
		bufferCopyRegion.imageOffset.y = Start[1];

		VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		vks::tools::setImageLayout(
			cmdBuffer,
			image,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		vkCmdCopyBufferToImage(
			cmdBuffer,
			WritableChunk.buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion);

		vks::tools::setImageLayout(
			cmdBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);
	}
	///**
	//* Load a 2D texture array including all mip levels
	//*
	//* @param filename File to load (supports .ktx)
	//* @param format Vulkan format of the image data stored in the file
	//* @param device Vulkan device to create the texture on
	//* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	//* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	//* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	//*
	//*/
	//void Texture2DArray::loadFromFile(std::string filename, VkFormat format, vks::VulkanDevice *device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
	//{
	//	ktxTexture* ktxTexture;
	//	ktxResult result = loadKTXFile(filename, &ktxTexture);
	//	assert(result == KTX_SUCCESS);

	//	this->device = device;
	//	width = ktxTexture->baseWidth;
	//	height = ktxTexture->baseHeight;
	//	layerCount = ktxTexture->numLayers;
	//	mipLevels = ktxTexture->numLevels;

	//	ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
	//	ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

	//	VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
	//	VkMemoryRequirements memReqs;

	//	// Create a host-visible staging buffer that contains the raw image data
	//	VkBuffer stagingBuffer;
	//	VkDeviceMemory stagingMemory;

	//	VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
	//	bufferCreateInfo.size = ktxTextureSize;
	//	// This buffer is used as a transfer source for the buffer copy
	//	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	//	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	//	VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	//	// Get memory requirements for the staging buffer (alignment, memory type bits)
	//	vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

	//	memAllocInfo.allocationSize = memReqs.size;
	//	// Get memory type index for a host visible buffer
	//	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	//	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	//	VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

	//	// Copy texture data into staging buffer
	//	uint8_t *data;
	//	VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
	//	memcpy(data, ktxTextureData, ktxTextureSize);
	//	vkUnmapMemory(device->logicalDevice, stagingMemory);

	//	// Setup buffer copy regions for each layer including all of its miplevels
	//	std::vector<VkBufferImageCopy> bufferCopyRegions;

	//	for (uint32_t layer = 0; layer < layerCount; layer++)
	//	{
	//		for (uint32_t level = 0; level < mipLevels; level++)
	//		{
	//			ktx_size_t offset;
	//			KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, layer, 0, &offset);
	//			assert(result == KTX_SUCCESS);

	//			VkBufferImageCopy bufferCopyRegion = {};
	//			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//			bufferCopyRegion.imageSubresource.mipLevel = level;
	//			bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
	//			bufferCopyRegion.imageSubresource.layerCount = 1;
	//			bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
	//			bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
	//			bufferCopyRegion.imageExtent.depth = 1;
	//			bufferCopyRegion.bufferOffset = offset;

	//			bufferCopyRegions.push_back(bufferCopyRegion);
	//		}
	//	}

	//	// Create optimal tiled target image
	//	VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
	//	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	//	imageCreateInfo.format = format;
	//	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	//	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	//	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//	imageCreateInfo.extent = { width, height, 1 };
	//	imageCreateInfo.usage = imageUsageFlags;
	//	// Ensure that the TRANSFER_DST bit is set for staging
	//	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	//	{
	//		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	//	}
	//	imageCreateInfo.arrayLayers = layerCount;
	//	imageCreateInfo.mipLevels = mipLevels;

	//	VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

	//	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

	//	memAllocInfo.allocationSize = memReqs.size;
	//	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
	//	VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

	//	// Use a separate command buffer for texture loading
	//	VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	//	// Image barrier for optimal image (target)
	//	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	//	VkImageSubresourceRange subresourceRange = {};
	//	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//	subresourceRange.baseMipLevel = 0;
	//	subresourceRange.levelCount = mipLevels;
	//	subresourceRange.layerCount = layerCount;

	//	vks::tools::setImageLayout(
	//		copyCmd,
	//		image,
	//		VK_IMAGE_LAYOUT_UNDEFINED,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		subresourceRange);

	//	// Copy the layers and mip levels from the staging buffer to the optimal tiled image
	//	vkCmdCopyBufferToImage(
	//		copyCmd,
	//		stagingBuffer,
	//		image,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		static_cast<uint32_t>(bufferCopyRegions.size()),
	//		bufferCopyRegions.data());

	//	// Change texture image layout to shader read after all faces have been copied
	//	this->imageLayout = imageLayout;
	//	vks::tools::setImageLayout(
	//		copyCmd,
	//		image,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		imageLayout,
	//		subresourceRange);

	//	device->flushCommandBuffer(copyCmd, copyQueue);

	//	// Create sampler
	//	VkSamplerCreateInfo samplerCreateInfo = vks::initializers::samplerCreateInfo();
	//	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	//	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	//	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	//	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	//	samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
	//	samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
	//	samplerCreateInfo.mipLodBias = 0.0f;
	//	samplerCreateInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
	//	samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
	//	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	//	samplerCreateInfo.minLod = 0.0f;
	//	samplerCreateInfo.maxLod = (float)mipLevels;
	//	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	//	VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

	//	// Create image view
	//	VkImageViewCreateInfo viewCreateInfo = vks::initializers::imageViewCreateInfo();
	//	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	//	viewCreateInfo.format = format;
	//	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	//	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	//	viewCreateInfo.subresourceRange.layerCount = layerCount;
	//	viewCreateInfo.subresourceRange.levelCount = mipLevels;
	//	viewCreateInfo.image = image;
	//	VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

	//	// Clean up staging resources
	//	ktxTexture_Destroy(ktxTexture);
	//	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
	//	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

	//	// Update descriptor image info member that can be used for setting up descriptor sets
	//	updateDescriptor();
	//}

	///**
	//* Load a cubemap texture including all mip levels from a single file
	//*
	//* @param filename File to load (supports .ktx)
	//* @param format Vulkan format of the image data stored in the file
	//* @param device Vulkan device to create the texture on
	//* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	//* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	//* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	//*
	//*/
	//void TextureCubeMap::loadFromFile(std::string filename, VkFormat format, vks::VulkanDevice *device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
	//{
	//	ktxTexture* ktxTexture;
	//	ktxResult result = loadKTXFile(filename, &ktxTexture);
	//	assert(result == KTX_SUCCESS);

	//	this->device = device;
	//	width = ktxTexture->baseWidth;
	//	height = ktxTexture->baseHeight;
	//	mipLevels = ktxTexture->numLevels;

	//	ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
	//	ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

	//	VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
	//	VkMemoryRequirements memReqs;

	//	// Create a host-visible staging buffer that contains the raw image data
	//	VkBuffer stagingBuffer;
	//	VkDeviceMemory stagingMemory;

	//	VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
	//	bufferCreateInfo.size = ktxTextureSize;
	//	// This buffer is used as a transfer source for the buffer copy
	//	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	//	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	//	VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	//	// Get memory requirements for the staging buffer (alignment, memory type bits)
	//	vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

	//	memAllocInfo.allocationSize = memReqs.size;
	//	// Get memory type index for a host visible buffer
	//	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	//	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
	//	VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

	//	// Copy texture data into staging buffer
	//	uint8_t *data;
	//	VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
	//	memcpy(data, ktxTextureData, ktxTextureSize);
	//	vkUnmapMemory(device->logicalDevice, stagingMemory);

	//	// Setup buffer copy regions for each face including all of its mip levels
	//	std::vector<VkBufferImageCopy> bufferCopyRegions;

	//	for (uint32_t face = 0; face < 6; face++)
	//	{
	//		for (uint32_t level = 0; level < mipLevels; level++)
	//		{
	//			ktx_size_t offset;
	//			KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
	//			assert(result == KTX_SUCCESS);

	//			VkBufferImageCopy bufferCopyRegion = {};
	//			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//			bufferCopyRegion.imageSubresource.mipLevel = level;
	//			bufferCopyRegion.imageSubresource.baseArrayLayer = face;
	//			bufferCopyRegion.imageSubresource.layerCount = 1;
	//			bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
	//			bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
	//			bufferCopyRegion.imageExtent.depth = 1;
	//			bufferCopyRegion.bufferOffset = offset;

	//			bufferCopyRegions.push_back(bufferCopyRegion);
	//		}
	//	}

	//	// Create optimal tiled target image
	//	VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
	//	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	//	imageCreateInfo.format = format;
	//	imageCreateInfo.mipLevels = mipLevels;
	//	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	//	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	//	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//	imageCreateInfo.extent = { width, height, 1 };
	//	imageCreateInfo.usage = imageUsageFlags;
	//	// Ensure that the TRANSFER_DST bit is set for staging
	//	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	//	{
	//		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	//	}
	//	// Cube faces count as array layers in Vulkan
	//	imageCreateInfo.arrayLayers = 6;
	//	// This flag is required for cube map images
	//	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;


	//	VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

	//	vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

	//	memAllocInfo.allocationSize = memReqs.size;
	//	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
	//	VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

	//	// Use a separate command buffer for texture loading
	//	VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	//	// Image barrier for optimal image (target)
	//	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	//	VkImageSubresourceRange subresourceRange = {};
	//	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//	subresourceRange.baseMipLevel = 0;
	//	subresourceRange.levelCount = mipLevels;
	//	subresourceRange.layerCount = 6;

	//	vks::tools::setImageLayout(
	//		copyCmd,
	//		image,
	//		VK_IMAGE_LAYOUT_UNDEFINED,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		subresourceRange);

	//	// Copy the cube map faces from the staging buffer to the optimal tiled image
	//	vkCmdCopyBufferToImage(
	//		copyCmd,
	//		stagingBuffer,
	//		image,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		static_cast<uint32_t>(bufferCopyRegions.size()),
	//		bufferCopyRegions.data());

	//	// Change texture image layout to shader read after all faces have been copied
	//	this->imageLayout = imageLayout;
	//	vks::tools::setImageLayout(
	//		copyCmd,
	//		image,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		imageLayout,
	//		subresourceRange);

	//	device->flushCommandBuffer(copyCmd, copyQueue);

	//	// Create sampler
	//	VkSamplerCreateInfo samplerCreateInfo = vks::initializers::samplerCreateInfo();
	//	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	//	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	//	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	//	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	//	samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
	//	samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
	//	samplerCreateInfo.mipLodBias = 0.0f;
	//	samplerCreateInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
	//	samplerCreateInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
	//	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	//	samplerCreateInfo.minLod = 0.0f;
	//	samplerCreateInfo.maxLod = (float)mipLevels;
	//	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	//	VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

	//	// Create image view
	//	VkImageViewCreateInfo viewCreateInfo = vks::initializers::imageViewCreateInfo();
	//	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	//	viewCreateInfo.format = format;
	//	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	//	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	//	viewCreateInfo.subresourceRange.layerCount = 6;
	//	viewCreateInfo.subresourceRange.levelCount = mipLevels;
	//	viewCreateInfo.image = image;
	//	VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &view));

	//	// Clean up staging resources
	//	ktxTexture_Destroy(ktxTexture);
	//	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
	//	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

	//	// Update descriptor image info member that can be used for setting up descriptor sets
	//	updateDescriptor();
	//}

}
