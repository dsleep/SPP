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
		auto VGD = dynamic_cast<VulkanGraphicsDevice*>(_owner);
		vks::debugmarker::setImageName(VGD->GetVKDevice(), _image->Get(), InName);
	}

	VkFormat TextureFormatToVKFormat(TextureFormat InFormat, bool isSRGB)
	{
		switch (InFormat)
		{
		case TextureFormat::R8:
			return VK_FORMAT_R8_UNORM;
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
		case TextureFormat::D32_S8:
			return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case TextureFormat::R32F:
			return VK_FORMAT_R32_SFLOAT;
		case TextureFormat::S8:
			return VK_FORMAT_S8_UINT;

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
		auto VGD = dynamic_cast<VulkanGraphicsDevice*>(_owner); 
		auto TotalTextureSize = InTextureAsset.GetTotalSize();
		auto copyQueue = VGD->GetGraphicsQueue();
		auto device = VGD->GetVKSVulkanDevice();

		_texformat = TextureFormatToVKFormat(_format, _bIsSRGB);
		_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		_usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		_allocate();
		
		// Get device properties for the requested texture format
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(device->physicalDevice, _texformat, &formatProperties);

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs = {};

		auto activeFrame = VGD->GetActiveFrame();
		auto& perFrameScratchBuffer = VGD->GetPerFrameScratchBuffer();
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

		auto& copyCmd = VGD->GetCopyCommandBuffer();

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

		VGD->SubmitCopyCommands();
	}

	void VulkanTexture::_allocate()
	{
		//normal or cubemap
		SE_ASSERT(_faceCount == 1 || _faceCount == 6);
		bool IsCubemap = (_faceCount == 6);


		auto VGD = dynamic_cast<VulkanGraphicsDevice*>(_owner);
		auto copyQueue = VGD->GetGraphicsQueue();
		auto device = VGD->GetVKSVulkanDevice();
		
		VkFilter filter = VK_FILTER_NEAREST;

		VkImageAspectFlags aspectMask = VK_FLAGS_NONE;
		VkImageAspectFlags viewAspectMask = VK_FLAGS_NONE;

		if (isDepthStencil())
		{
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			viewAspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		else
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewAspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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
		imageCreateInfo.initialLayout = _initialLayout;
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
		viewCreateInfo.subresourceRange = { viewAspectMask, 0, (uint32_t)_mipLevels, 0, (uint32_t)_faceCount };
		viewCreateInfo.image = _image->Get();
		_view = std::make_unique< SafeVkImageView >(_owner, viewCreateInfo);
		// Update descriptor _image info member that can be used for setting up descriptor sets
		updateDescriptor();

		// Use a separate command buffer for texture loading
		auto& copyCmd = VGD->GetCopyCommandBuffer();

		_subresourceRange = { aspectMask, 0, _mipLevels, 0, _faceCount };

		if (_initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			vks::tools::setImageLayout(
				copyCmd,
				_image->Get(),
				VK_IMAGE_LAYOUT_UNDEFINED,
				_imageLayout,
				_subresourceRange);
		}

		VGD->SubmitCopyCommands();
	}

	VulkanTexture::VulkanTexture(GraphicsDevice* InOwner, 
		int32_t Width, int32_t Height, 
		int32_t MipLevelCount, int32_t FaceCount, 
		TextureFormat Format, VkImageUsageFlags UsageFlags, VkImageLayout InitialLayout)
		: GPUTexture(InOwner, Width, Height, MipLevelCount, FaceCount, Format)
	{
		_imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		_initialLayout = InitialLayout;

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

			auto newView = Make_GPU(SafeVkImageView, _owner, viewCreateInfo);
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

		auto VGD = dynamic_cast<VulkanGraphicsDevice*>(_owner);
		auto& perFrameScratchBuffer = VGD->GetPerFrameScratchBuffer();
		auto& cmdBuffer = VGD->GetCopyCommandBuffer();
		auto activeFrame = VGD->GetActiveFrame();

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
}
