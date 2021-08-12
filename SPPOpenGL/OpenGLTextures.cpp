// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "OpenGLTextures.h"

namespace SPP
{
	OpenGLTexture::OpenGLTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo)
		: GPUTexture(Width, Height, Format, RawData, InMetaInfo)
	{
		glGenTextures(1, &_textureID);
	}

	OpenGLTexture::~OpenGLTexture()
	{
		glDeleteTextures(1, &_textureID);
	}

	void OpenGLTexture::UploadToGpu() 
	{
		SE_ASSERT(_rawImgData);

		// "Bind" the newly created texture : all future texture functions will modify this texture
		glBindTexture(GL_TEXTURE_2D, _textureID);

		// Give the image to OpenGL
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0, GL_RGB, GL_UNSIGNED_BYTE, _rawImgData->GetElementData());

		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		// ... which requires mipmaps. Generate them automatically.
		//glGenerateMipmap(GL_TEXTURE_2D);
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