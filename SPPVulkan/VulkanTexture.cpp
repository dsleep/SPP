// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#include <VulkanTexture.h>
#include "VulkanResources.h"
#include "VulkanDebug.h"
#include "SPPTextures.h"

namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	void VulkanTexture::updateDescriptor()
	{
		_descriptor.sampler = _sampler->Get();
		_descriptor.imageView = _view->Get();
		_descriptor.imageLayout = _imageLayout;
	}

	void VulkanTexture::destroy()
	{
		_view.reset();
		_image.reset();
		_sampler.reset();
		_deviceMemory.reset();
	}

	VulkanTexture::VulkanTexture(GraphicsDevice* InOwner,
		int32_t Width,
		int32_t Height,
		TextureFormat Format,
		std::shared_ptr< ArrayResource > RawData,
		std::shared_ptr< ImageMeta > InMetaInfo)
		: VulkanTexture(InOwner,
			TextureAsset
			{
			 .orgFileName = "",
			 .width = Width,
			 .height = Height,

			 .bSRGB = false,
			 .format = Format,
			 .faceData = { 
					{ std::shared_ptr< TextureFace >(new TextureFace{
						.mipData = { { RawData } } }) 
					} 
				}
			})
	{
	}

	void VulkanTexture::SetName(const char* InName)
	{
		vks::debugmarker::setImageName(GGlobalVulkanDevice, _image->Get(), InName);
	}

	VkFormat TextureFormatToVKFormat(TextureFormat InFormat, bool isSRGB)
	{
		switch (InFormat)
		{
		case TextureFormat::RGB_888:
			return isSRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
		case TextureFormat::RGBA_8888:
			return isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat::BGRA_8888:
			return isSRGB ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
		case TextureFormat::RGB_BC1:
			return isSRGB ? VK_FORMAT_BC1_RGB_SRGB_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case TextureFormat::RGBA_BC7:
			return isSRGB ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;
		case TextureFormat::D32:
			return VK_FORMAT_D32_SFLOAT;
		case TextureFormat::R32F:
			return VK_FORMAT_R32_SFLOAT;
		case TextureFormat::R16G16F:
			return VK_FORMAT_R16G16_SFLOAT;
		case TextureFormat::R16G16B16A16F:
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		
		default:
			//TODO lazyness
			SE_ASSERT(false);
			break;
		}

		return VK_FORMAT_UNDEFINED;
	}

	VulkanTexture::VulkanTexture(GraphicsDevice* InOwner, const struct TextureAsset& InTextureAsset) : 
		GPUTexture(InOwner, InTextureAsset)
	{
		auto sfileName = stdfs::path(InTextureAsset.orgFileName).filename().generic_string();

		auto TotalTextureSize = InTextureAsset.GetTotalSize();
		auto copyQueue = GGlobalVulkanGI->GetGraphicsQueue();
		auto device = GGlobalVulkanGI->GetVKSVulkanDevice();

		_texformat = TextureFormatToVKFormat(_format, _bIsSRGB);
		_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		_usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		_allocate();
		
		// Get device properties for the requested texture format
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(device->physicalDevice, _texformat, &formatProperties);

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs = {};

		auto activeFrame = GGlobalVulkanGI->GetActiveFrame();
		auto& perFrameScratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();
		auto WritableChunk = perFrameScratchBuffer.GetWritable((uint32_t)TotalTextureSize, activeFrame);

		// Setup buffer copy regions for each mip level
		std::vector<VkBufferImageCopy> bufferCopyRegions;

		//normal or cubemap
		SE_ASSERT(_faceData.size() == 1 || _faceData.size() == 6);
		bool IsCubemap = (_faceData.size() == 6);

		// layer and faces kinda synonymous
		uint32_t LayerCount = (uint32_t)_faceData.size();

		uint32_t currentOffset = 0;
		for (uint32_t faceIter = 0; faceIter < _faceData.size(); faceIter++)
		{
			auto& curFace = _faceData[faceIter];
			
			for (uint32_t mipIter = 0; mipIter < curFace->mipData.size(); mipIter++)
			{
				auto& curMip = curFace->mipData[mipIter];

				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = _subresourceRange.aspectMask;
				bufferCopyRegion.imageSubresource.mipLevel = mipIter;
				bufferCopyRegion.imageSubresource.baseArrayLayer = faceIter;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = std::max< int32_t>(1u, _width >> mipIter);
				bufferCopyRegion.imageExtent.height = std::max< int32_t>(1u, _height >> mipIter);
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = currentOffset + WritableChunk.offsetFromBase;

				memcpy(WritableChunk.cpuAddrWithOffset + currentOffset, curMip->GetElementData(), curMip->GetTotalSize());
				currentOffset += (uint32_t)curMip->GetTotalSize();

				bufferCopyRegions.push_back(bufferCopyRegion);
			}
		}

		auto& copyCmd = GGlobalVulkanGI->GetCopyCommandBuffer();

		// Image barrier for optimal _image (target)
		// Optimal _image will be used as destination for the copy
		vks::tools::setImageLayout(
			copyCmd,
			_image->Get(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			_subresourceRange);

		// Copy mip levels from staging buffer
		vkCmdCopyBufferToImage(
			copyCmd,
			WritableChunk.buffer,
			_image->Get(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);

		// Change texture _image layout to shader read after all mip levels have been copied		
		vks::tools::setImageLayout(
			copyCmd,
			_image->Get(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			_imageLayout,
			_subresourceRange);

		GGlobalVulkanGI->SubmitCopyCommands();
	}

	void VulkanTexture::_allocate()
	{
		//normal or cubemap
		SE_ASSERT(_faceCount == 1 || _faceCount == 6);
		bool IsCubemap = (_faceCount == 6);

		auto copyQueue = GGlobalVulkanGI->GetGraphicsQueue();
		auto device = GGlobalVulkanGI->GetVKSVulkanDevice();
		
		VkFilter filter = VK_FILTER_NEAREST;

		VkImageAspectFlags aspectMask = VK_FLAGS_NONE;

		if (hasDepth())
		{
			aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (hasStencil())
		{
			aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		if (aspectMask == VK_FLAGS_NONE)
		{
			aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
		}

		// Create optimal tiled target _image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = _texformat;
		imageCreateInfo.mipLevels = _mipLevels;
		imageCreateInfo.arrayLayers = _faceCount;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { (uint32_t)_width, (uint32_t)_height, 1 };
		imageCreateInfo.usage = _usageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		{
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		if (IsCubemap)
		{
			// This flag is required for cube map images
			imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}
		_image = std::make_unique< SafeVkImage >(_owner, imageCreateInfo);

		vks::debugmarker::setImageName(device->logicalDevice, _image->Get(), "VT_Empty");

		VkMemoryRequirements memReqs = { 0 };
		vkGetImageMemoryRequirements(device->logicalDevice, _image->Get(), &memReqs);

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		_deviceMemory = std::make_unique< SafeVkDeviceMemory >(_owner, memAllocInfo);
		VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, _image->Get(), _deviceMemory->Get(), 0));

		_imageByteSize = (uint32_t) memReqs.size;

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
		samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		_sampler = std::make_unique< SafeVkSampler >(_owner, samplerCreateInfo);

		// Create _image _view
		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.pNext = NULL;
		viewCreateInfo.viewType = IsCubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = _texformat;
		viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		viewCreateInfo.subresourceRange = { aspectMask, 0, (uint32_t)_mipLevels, 0, (uint32_t)_faceCount };
		viewCreateInfo.image = _image->Get();
		_view = std::make_unique< SafeVkImageView >(_owner, viewCreateInfo);
		// Update descriptor _image info member that can be used for setting up descriptor sets
		updateDescriptor();

		// Use a separate command buffer for texture loading
		auto& copyCmd = GGlobalVulkanGI->GetCopyCommandBuffer();

		_subresourceRange = { aspectMask, 0, _mipLevels, 0, _faceCount };

		vks::tools::setImageLayout(
			copyCmd,
			_image->Get(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			_subresourceRange);

		GGlobalVulkanGI->SubmitCopyCommands();
	}

	VulkanTexture::VulkanTexture(GraphicsDevice* InOwner, 
		int32_t Width, int32_t Height, 
		int32_t MipLevelCount, int32_t FaceCount, 
		TextureFormat Format, VkImageUsageFlags UsageFlags)
		: GPUTexture(InOwner, Width, Height, MipLevelCount, FaceCount, Format)
	{
		_imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		if (UsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		else if (UsageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			_imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}

		_texformat = TextureFormatToVKFormat(Format, false);
		_usageFlags = UsageFlags;

		_allocate();
	}

	VulkanTexture::VulkanTexture(GraphicsDevice* InOwner, int32_t Width, int32_t Height, TextureFormat Format) :
		VulkanTexture(InOwner, Width, Height, 1, 1, Format, VK_IMAGE_USAGE_SAMPLED_BIT |
			VK_IMAGE_USAGE_STORAGE_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	{
	}

	std::vector< GPUReferencer< SafeVkImageView > > VulkanTexture::GetMipChainViews() 
	{
		std::vector< GPUReferencer< SafeVkImageView > > oViews;
		for (int32_t Iter = 0; Iter < _mipLevels; Iter++)
		{
			VkImageViewCreateInfo viewCreateInfo = {};
			viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewCreateInfo.pNext = NULL;
			viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewCreateInfo.format = _texformat;
			viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			viewCreateInfo.subresourceRange = { _subresourceRange.aspectMask, (uint32_t)Iter, 1, 0, 1 };			
			viewCreateInfo.image = _image->Get();

			auto newView = Make_GPU(SafeVkImageView, GGlobalVulkanGI, viewCreateInfo);
			oViews.push_back(newView);
		}

		return oViews;
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
		bufferCopyRegion.imageSubresource.aspectMask = _subresourceRange.aspectMask;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = Extents[0];
		bufferCopyRegion.imageExtent.height = Extents[1];
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = WritableChunk.offsetFromBase;
		bufferCopyRegion.imageOffset.x = Start[0];
		bufferCopyRegion.imageOffset.y = Start[1];

		vks::tools::setImageLayout(
			cmdBuffer,
			_image->Get(),
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			_subresourceRange);

		vkCmdCopyBufferToImage(
			cmdBuffer,
			WritableChunk.buffer,
			_image->Get(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&bufferCopyRegion);

		vks::tools::setImageLayout(
			cmdBuffer,
			_image->Get(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			_subresourceRange);
	}

	bool VulkanTexture::hasDepth()
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
		return std::find(formats.begin(), formats.end(), _texformat) != std::end(formats);
	}

	bool VulkanTexture::hasStencil()
	{
		std::vector<VkFormat> formats =
		{
			VK_FORMAT_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
		};
		return std::find(formats.begin(), formats.end(), _texformat) != std::end(formats);
	}
	///**
	//* Load a 2D texture array including all mip levels
	//*
	//* @param filename File to load (supports .ktx)
	//* @param format Vulkan format of the _image data stored in the file
	//* @param device Vulkan device to create the texture on
	//* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	//* @param (Optional) imageUsageFlags Usage flags for the texture's _image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
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

	//	// Create a host-visible staging buffer that contains the raw _image data
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

	//	// Create optimal tiled target _image
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

	//	VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &_image));

	//	vkGetImageMemoryRequirements(device->logicalDevice, _image, &memReqs);

	//	memAllocInfo.allocationSize = memReqs.size;
	//	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &_deviceMemory));
	//	VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, _image, _deviceMemory, 0));

	//	// Use a separate command buffer for texture loading
	//	VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	//	// Image barrier for optimal _image (target)
	//	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	//	VkImageSubresourceRange subresourceRange = {};
	//	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//	subresourceRange.baseMipLevel = 0;
	//	subresourceRange.levelCount = mipLevels;
	//	subresourceRange.layerCount = layerCount;

	//	vks::tools::setImageLayout(
	//		copyCmd,
	//		_image,
	//		VK_IMAGE_LAYOUT_UNDEFINED,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		subresourceRange);

	//	// Copy the layers and mip levels from the staging buffer to the optimal tiled _image
	//	vkCmdCopyBufferToImage(
	//		copyCmd,
	//		stagingBuffer,
	//		_image,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		static_cast<uint32_t>(bufferCopyRegions.size()),
	//		bufferCopyRegions.data());

	//	// Change texture _image layout to shader read after all faces have been copied
	//	this->imageLayout = imageLayout;
	//	vks::tools::setImageLayout(
	//		copyCmd,
	//		_image,
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

	//	// Create _image _view
	//	VkImageViewCreateInfo viewCreateInfo = vks::initializers::imageViewCreateInfo();
	//	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	//	viewCreateInfo.format = format;
	//	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	//	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	//	viewCreateInfo.subresourceRange.layerCount = layerCount;
	//	viewCreateInfo.subresourceRange.levelCount = mipLevels;
	//	viewCreateInfo._image = _image;
	//	VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &_view));

	//	// Clean up staging resources
	//	ktxTexture_Destroy(ktxTexture);
	//	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
	//	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

	//	// Update descriptor _image info member that can be used for setting up descriptor sets
	//	updateDescriptor();
	//}

	///**
	//* Load a cubemap texture including all mip levels from a single file
	//*
	//* @param filename File to load (supports .ktx)
	//* @param format Vulkan format of the _image data stored in the file
	//* @param device Vulkan device to create the texture on
	//* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	//* @param (Optional) imageUsageFlags Usage flags for the texture's _image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
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

	//	// Create a host-visible staging buffer that contains the raw _image data
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

	//	// Create optimal tiled target _image
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


	//	VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &_image));

	//	vkGetImageMemoryRequirements(device->logicalDevice, _image, &memReqs);

	//	memAllocInfo.allocationSize = memReqs.size;
	//	memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &_deviceMemory));
	//	VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, _image, _deviceMemory, 0));

	//	// Use a separate command buffer for texture loading
	//	VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	//	// Image barrier for optimal _image (target)
	//	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	//	VkImageSubresourceRange subresourceRange = {};
	//	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//	subresourceRange.baseMipLevel = 0;
	//	subresourceRange.levelCount = mipLevels;
	//	subresourceRange.layerCount = 6;

	//	vks::tools::setImageLayout(
	//		copyCmd,
	//		_image,
	//		VK_IMAGE_LAYOUT_UNDEFINED,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		subresourceRange);

	//	// Copy the cube map faces from the staging buffer to the optimal tiled _image
	//	vkCmdCopyBufferToImage(
	//		copyCmd,
	//		stagingBuffer,
	//		_image,
	//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//		static_cast<uint32_t>(bufferCopyRegions.size()),
	//		bufferCopyRegions.data());

	//	// Change texture _image layout to shader read after all faces have been copied
	//	this->imageLayout = imageLayout;
	//	vks::tools::setImageLayout(
	//		copyCmd,
	//		_image,
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

	//	// Create _image _view
	//	VkImageViewCreateInfo viewCreateInfo = vks::initializers::imageViewCreateInfo();
	//	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	//	viewCreateInfo.format = format;
	//	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	//	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	//	viewCreateInfo.subresourceRange.layerCount = 6;
	//	viewCreateInfo.subresourceRange.levelCount = mipLevels;
	//	viewCreateInfo._image = _image;
	//	VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &_view));

	//	// Clean up staging resources
	//	ktxTexture_Destroy(ktxTexture);
	//	vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
	//	vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

	//	// Update descriptor _image info member that can be used for setting up descriptor sets
	//	updateDescriptor();
	//}

}
