// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "DX12Device.h"

namespace SPP
{
	class D3D12Texture : public GPUTexture
	{
	protected:
		ComPtr<ID3D12Resource> _texture;
		ComPtr<ID3D12Resource> _textureUpload;
		D3D12_RESOURCE_DESC _desc = {};

	public:

		ID3D12Resource* GetTexture();
		const D3D12_RESOURCE_DESC &GetDescription();
		virtual void UploadToGpu() override;

		D3D12Texture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);
			
		virtual ~D3D12Texture() { }
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

	class D3D12RenderTarget : public GPURenderTarget
	{
	protected:
		ComPtr<ID3D12Resource> _texture;
		D3D12_CPU_DESCRIPTOR_HANDLE _srvDescriptor;
		D3D12_CPU_DESCRIPTOR_HANDLE _rtvDescriptor;

	public:
		virtual void UploadToGpu() override { }
		D3D12RenderTarget(int32_t Width, int32_t Height, TextureFormat Format);

		virtual ~D3D12RenderTarget() { }
	};
}