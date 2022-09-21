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

#include "SPPPlatformCore.h"

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

	void GenerateMipMapCompressedTexture(const char *InPath, const char *OutPath, bool bHasAlpha)
	{
		auto FullBinPath = SPP::GRootPath + "/3rdParty/compressonator/bin/compressonatorcli.exe";

		auto CommandString = std::string_format("-miplevels 20 -fd %s \"%s\" \"%s\"",
			bHasAlpha ? "BC7" : "BC1 -AlphaThreshold 255",
			InPath,
			OutPath
		);

		{
			auto BinProcess = CreatePlatformProcess(FullBinPath.c_str(), CommandString.c_str(), false, true);

			if (BinProcess->IsValid())
			{
				auto ProcessOutput = BinProcess->GetOutput();

				if (!ProcessOutput.empty())
				{
					SPP_LOG(LOG_TEXTURES, LOG_INFO, "%s", ProcessOutput.c_str());
				}
			}
		}
	}

	TextureFormat VKFormatToTextureFormat(uint32_t InVKFormat, bool &IsSRGB)
	{
		//VK_FORMAT_BC1_RGB_UNORM_BLOCK = 131,
		//VK_FORMAT_BC1_RGB_SRGB_BLOCK = 132,
		//VK_FORMAT_BC7_UNORM_BLOCK = 145,
		//VK_FORMAT_BC7_SRGB_BLOCK = 146,

		switch (InVKFormat)
		{
		case 131:
			return TextureFormat::RGB_BC1;
			IsSRGB = false;
			break;
		case 132:
			return TextureFormat::RGB_BC1;
			IsSRGB = true;
			break;
		case 145:
			return TextureFormat::RGBA_BC7;
			IsSRGB = false;
			break;
		case 146:
			return TextureFormat::RGBA_BC7;
			IsSRGB = true;
			break;
		}

		SE_ASSERT(false);

		return TextureFormat::UNKNOWN;
	}

	bool TextureAsset::LoadFromDisk(const char* inFileName)
	{
		static_assert(sizeof(SimpleRGB) == 3);
		static_assert(sizeof(SimpleRGBA) == 4);

		std::string FileName = inFileName;

		orgFileName = FileName;

		width = 0;
		height = 0;
		faceData.clear();

		SPP_LOG(LOG_TEXTURES, LOG_INFO, "Loading Texture: %s", inFileName);

		auto extension = stdfs::path(FileName).extension().generic_string();
		inlineToUpper(extension);

		bool IsKTX = str_equals(extension, ".KTX");
		bool IsKTX2 = str_equals(extension, ".KTX2");
		bool IsDDS = str_equals(extension, ".DDS");

		
		//if (!IsKTX2)
		//{
		//	auto ktxExt2 = stdfs::path(".KTX2");
		//	FileName = stdfs::path(FileName).replace_extension(ktxExt2).generic_string();
		//	GenerateMipMapCompressedTexture(inFileName, FileName.c_str(), false);
		//	IsKTX2 = true;
		//}


		if (IsKTX || IsKTX2)
		{
			KTX_error_code result;
			
			std::vector<uint8_t> fileData;
			if (LoadFileToArray(FileName.c_str(), fileData))
			{
				ktxTexture2* texture2 = nullptr;

				KTX_error_code result = ktxTexture2_CreateFromMemory((const ktx_uint8_t*)fileData.data(), fileData.size(),
					KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
					&texture2);

				if (result != KTX_SUCCESS) return false;
				if (texture2->classId != ktxTexture2_c) return false;

				auto ktxTextureData = ktxTexture_GetData(ktxTexture(texture2));
				auto ktxTextureSize = ktxTexture_GetDataSize(ktxTexture(texture2));

				auto vkFormat = texture2->vkFormat;
				
				width = texture2->baseWidth;
				height = texture2->baseHeight;
				format = VKFormatToTextureFormat(vkFormat, bSRGB);

				auto mipLevels = texture2->numLevels;

				SE_ASSERT(texture2->numDimensions == 2);
				SE_ASSERT(texture2->numLayers == 1);

				for (ktx_uint32_t faceIter = 0; faceIter < texture2->numFaces; faceIter++)
				{
					auto curFace = std::make_shared< TextureFace >();

					for (ktx_uint32_t level = 0; level < texture2->numLevels; level++)
					{
						ktx_size_t levelOffset;
						result = ktxTexture_GetImageOffset(ktxTexture(texture2), level, 0, faceIter, &levelOffset);
						SE_ASSERT(result == KTX_SUCCESS);
						auto levelSize = ktxTexture_GetImageSize(ktxTexture(texture2), level);
						SE_ASSERT(result == KTX_SUCCESS);

						auto rawImgData = std::make_shared< ArrayResource >();
						curFace->mipData.push_back(rawImgData);

						auto& rawData = rawImgData->GetRawByteArray();
						rawData.resize(levelSize);
						memcpy(rawData.data(), ktxTextureData + levelOffset, levelSize);
					}

					faceData.push_back(curFace);
				}

				ktxTexture_Destroy(ktxTexture(texture2));

				return true;
			}
		}
		else if (IsDDS)
		{
			auto curFace = std::make_shared< TextureFace >();
			auto rawImgData = std::make_shared< ArrayResource >();
			curFace->mipData.push_back(rawImgData);
			faceData.push_back(curFace);

			const DirectX::DDS_HEADER* header = nullptr;
			const uint8_t* bitData = nullptr;
			size_t bitSize = 0;
			auto& ddsData = rawImgData->GetRawByteArray();

			if (LoadDDSTextureDataFromFile(FileName.c_str(), ddsData, &header, &bitData, &bitSize))
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
			auto curFace = std::make_shared< TextureFace >();
			auto rawImgData = std::make_shared< ArrayResource >();
			curFace->mipData.push_back(rawImgData);
			faceData.push_back(curFace);

			int x, y, n;
			unsigned char* pixels = stbi_load(FileName.c_str(), &x, &y, &n, 0);

			SE_ASSERT(pixels);

			if (pixels)
			{
				width = x;
				height = y;
				format = TextureFormat::RGBA_8888;

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