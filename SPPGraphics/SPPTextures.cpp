// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPTextures.h"

#include "SPPFileSystem.h"
#include "SPPString.h"
#include "SPPSTLUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

//#include "XTK12/DDS.h"

namespace SPP
{
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

#if 0
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

	bool TextureObject::Generate(int32_t InWidth, int32_t InHeight, TextureFormat InFormat)
	{
		_width = InWidth;
		_height = InHeight;
		_format = InFormat;

		_texture = SPP::CreateTexture(_width, _height, _format);

		return true;
	}

	bool TextureObject::LoadFromDisk(const AssetPath& FileName)
	{
		static_assert(sizeof(SimpleRGB) == 3);
		static_assert(sizeof(SimpleRGBA) == 4);

		_width = 0;
		_height = 0;
		_rawImgData = std::make_shared< ArrayResource >();

		auto extension = FileName.GetExtension();

		if (str_equals(extension, ".dds"))
		{
			const DirectX::DDS_HEADER* header = nullptr;
			const uint8_t* bitData = nullptr;
			size_t bitSize = 0;
			auto& ddsData = _rawImgData->GetRawByteArray();

			if (LoadDDSTextureDataFromFile(*FileName, ddsData, &header, &bitData, &bitSize))
			{
				_width = header->width;
				_height = header->height;

				auto ddsmeta = std::make_shared< DDSImageMeta>();
				ddsmeta->header = header;
				ddsmeta->bitData = bitData;
				ddsmeta->bitSize = bitSize;

				//_texture create gpu texture
				_texture = SPP::CreateTexture(_width, _height, TextureFormat::DDS_UNKNOWN, _rawImgData, ddsmeta);
			}
		}
		else
		{
			int x, y, n;
			unsigned char* pixels = stbi_load(*FileName, &x, &y, &n, 0);

			SE_ASSERT(pixels);

			if (pixels)
			{
				_width = x;
				_height = y;

				auto data = _rawImgData->InitializeFromType<SimpleRGBA>(x * y);

				if (n == 4)
				{
					memcpy(data, pixels, _rawImgData->GetTotalSize());
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
				_texture = SPP::CreateTexture(_width, _height, TextureFormat::RGBA_8888, _rawImgData);

				return true;
			}
		}

		return false;
	}
#endif
}