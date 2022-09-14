// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPTextures.h"
#include "SPPLogging.h"

#include "SPPFileSystem.h"
#include "SPPString.h"
#include "SPPSTLUtils.h"

#include <ktx.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// UGLY
#include "../SPPDX12/XTK12/DDS.h"

namespace SPP
{
	LogEntry LOG_TEXTURES("TEXTURES");

	bool SaveImageToFile(const char* FilePath,
		uint32_t Width,
		uint32_t Height,
		TextureFormat Format,
		const uint8_t *ImageData)
	{
		if (Format == TextureFormat::RGB_888)
		{
			auto FileExt = stdfs::path(FilePath).extension().generic_string();
			std::inlineToLower(FileExt);

			if (EndsWith(FileExt, "jpg"))
			{
				stbi_write_jpg(FilePath, Width, Height, 3, ImageData, 50);
			}
			else if (EndsWith(FileExt, "bmp"))
			{
				stbi_write_bmp(FilePath, Width, Height, 3, ImageData);
			}
			else if (EndsWith(FileExt, "tga"))
			{
				stbi_write_tga(FilePath, Width, Height, 3, ImageData);
			}
			else
			{
				stbi_write_png(FilePath, Width, Height, 3, ImageData, Width * 3);
			}

			return true;
		}
		else
		{
			return false;
		}
	}

	inline bool LoadDDSTextureDataFromFile(
		const char* fileName,
		std::vector<uint8_t>& ddsData,
		const DirectX::DDS_HEADER** header,
		const uint8_t** bitData,
		size_t* bitSize) noexcept
	{
		if (!header || !bitData || !bitSize)
		{
			return false;
		}

		*bitSize = 0;

		if (LoadFileToArray(fileName, ddsData) == false)
		{
			ddsData.clear();
			return false;
		}

		// DDS files always start with the same magic number ("DDS ")
		auto dwMagicNumber = *reinterpret_cast<const uint32_t*>(ddsData.data());
		if (dwMagicNumber != DirectX::DDS_MAGIC)
		{
			ddsData.clear();
			return false;
		}

		auto hdr = reinterpret_cast<const DirectX::DDS_HEADER*>(ddsData.data() + sizeof(uint32_t));

		// Verify header to validate DDS file
		if (hdr->size != sizeof(DirectX::DDS_HEADER) || hdr->ddspf.size != sizeof(DirectX::DDS_PIXELFORMAT))
		{
			ddsData.clear();
			return false;
		}

		// Check for DX10 extension
		bool bDXT10Header = false;
		if ((hdr->ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == hdr->ddspf.fourCC))
		{
			// Must be long enough for both headers and magic value
			if (ddsData.size() < (sizeof(uint32_t) + sizeof(DirectX::DDS_HEADER) + sizeof(DirectX::DDS_HEADER_DXT10)))
			{
				ddsData.clear();
				return false;
			}

			bDXT10Header = true;
		}

		// setup the pointers in the process request
		*header = hdr;
		auto offset = sizeof(uint32_t) + sizeof(DirectX::DDS_HEADER) + (bDXT10Header ? sizeof(DirectX::DDS_HEADER_DXT10) : 0u);
		*bitData = ddsData.data() + offset;
		*bitSize = ddsData.size() - offset;

		return true;
	}

	bool TextureAsset::Generate(int32_t InWidth, int32_t InHeight, TextureFormat InFormat)
	{
		width = InWidth;
		height = InHeight;
		format = InFormat;

		//TODO FIXME
		//_texture = GGI()->CreateTexture(_width, _height, _format);

		return true;
	}

	bool TextureAsset::LoadFromDisk(const char* FileName)
	{
		static_assert(sizeof(SimpleRGB) == 3);
		static_assert(sizeof(SimpleRGBA) == 4);

		width = 0;
		height = 0;
		rawImgData = std::make_shared< ArrayResource >();

		SPP_LOG(LOG_TEXTURES, LOG_INFO, "Loading Texture: %s", FileName);

		auto extension = stdfs::path(FileName).extension().generic_string();
		inlineToUpper(extension);

		bool IsKTX = str_equals(extension, ".KTX");
		bool IsKTX2 = str_equals(extension, ".KTX2");
		bool IsDDS = str_equals(extension, ".DDS");

		if (IsKTX || IsKTX2)
		{
			KTX_error_code result;
			
			std::vector<uint8_t> fileData;
			if (LoadFileToArray(FileName, fileData))
			{
				ktxTexture2* texture2 = nullptr;

				KTX_error_code result = ktxTexture2_CreateFromMemory((const ktx_uint8_t*)fileData.data(), fileData.size(),
					KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
					&texture2);

				if (result != KTX_SUCCESS) return false;
				if (texture2->classId != ktxTexture2_c) return false;

				auto vkFormat = texture2->vkFormat;
				ktx_uint32_t components = ktxTexture2_GetNumComponents(texture2);

				auto width = texture2->baseWidth;
				auto height = texture2->baseHeight;
				auto mipLevels = texture2->numLevels;

				for (ktx_uint32_t level = 0; level < texture2->numLevels; level++)
				{
					ktx_size_t levelOffset;
					result = ktxTexture_GetImageOffset(ktxTexture(texture2), level, 0, 0, &levelOffset);

					SE_ASSERT(result == KTX_SUCCESS);
				}

				ktxTexture_Destroy(ktxTexture(texture2));

				return true;
			}
		}
		else if (IsDDS)
		{
			const DirectX::DDS_HEADER* header = nullptr;
			const uint8_t* bitData = nullptr;
			size_t bitSize = 0;
			auto& ddsData = rawImgData->GetRawByteArray();

			if (LoadDDSTextureDataFromFile(FileName, ddsData, &header, &bitData, &bitSize))
			{
				width = header->width;
				height = header->height;

				auto ddsmeta = std::make_shared< DDSImageMeta>();
				ddsmeta->header = header;
				ddsmeta->bitData = bitData;
				ddsmeta->bitSize = bitSize;

				//_texture create gpu texture
				//TODO FIXME
				//_texture = GGI()->CreateTexture(_width, _height, TextureFormat::DDS_UNKNOWN, _rawImgData, ddsmeta);
				SPP_LOG(LOG_TEXTURES, LOG_INFO, " - loaded dds");

				return true;
			}
			else
			{
				SPP_LOG(LOG_TEXTURES, LOG_INFO, " - FAILED loading dds");
			}
		}
		else
		{
			int x, y, n;
			unsigned char* pixels = stbi_load(FileName, &x, &y, &n, 0);

			SE_ASSERT(pixels);

			if (pixels)
			{
				width = x;
				height = y;

				auto data = rawImgData->InitializeFromType<SimpleRGBA>(x * y);

				if (n == 4)
				{
					memcpy(data, pixels, rawImgData->GetTotalSize());
				}
				else
				{
					SE_ASSERT(n == 3);
					auto PixelCount = x * y;
					for (int32_t Iter = 0; Iter < PixelCount; Iter++)
					{
						SimpleRGB* src = (SimpleRGB*)pixels + Iter;
						SimpleRGBA& dst = data[Iter];

						dst.R = src->R;
						dst.G = src->G;
						dst.B = src->B;
						dst.A = 255;
					}
				}

				stbi_image_free(pixels);

				//_texture create gpu texture				
				//TODO FIXME
				//_texture = GGI()->CreateTexture(_width, _height, TextureFormat::RGBA_8888, _rawImgData);

				return true;
			}
		}

		return false;
	}
}