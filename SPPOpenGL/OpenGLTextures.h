// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "OpenGLDevice.h"

namespace SPP
{
	class OpenGLTexture : public GPUTexture
	{
	protected:

	public:


		OpenGLTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);


		virtual void UploadToGpu() override { }
			 
		//void CreateSRV()
		//{
		//	// Describe and create a SRV for the texture.
		//	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		//	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		//	srvDesc.Format = _desc.Format;
		//	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		//	srvDesc.Texture2D.MipLevels = 1;
		//	pd3dDevice->CreateShaderResourceView(_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
		//}
	};
}