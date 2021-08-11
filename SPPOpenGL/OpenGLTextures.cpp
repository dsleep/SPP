// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "OpenGLTextures.h"

namespace SPP
{
	OpenGLTexture::OpenGLTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo)
		: GPUTexture(Width, Height, Format, RawData, InMetaInfo)
	{

	}

	std::shared_ptr< GPUTexture > DX12_CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo)
	{
		return std::make_shared<OpenGLTexture>(Width, Height, Format, RawData, InMetaInfo);
	}

	std::shared_ptr< GPURenderTarget > DX12_CreateRenderTarget()
	{
		return nullptr;
	}

}