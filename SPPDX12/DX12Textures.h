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
		ComPtr<ID3D12DescriptorHeap> _cpuSrvDescriptor;
		D3D12_RESOURCE_DESC _desc = {};
		DXGI_FORMAT _dxFormat = DXGI_FORMAT_UNKNOWN;
		uint32_t _uniqueID = 0;

	public:

		ID3D12Resource* GetTexture();
		ID3D12DescriptorHeap* GetCPUDescriptor()
		{
			return _cpuSrvDescriptor.Get();
		}
		const D3D12_RESOURCE_DESC &GetDescription();
		virtual void UploadToGpu() override;

		D3D12Texture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);
		virtual ~D3D12Texture();
	};

	class D3D12RenderTarget : public GPURenderTarget
	{
	protected:
		ComPtr<ID3D12Resource> _texture;
		ComPtr<ID3D12DescriptorHeap> _cpuDescriptor;
		ComPtr<ID3D12DescriptorHeap> _cpuSrvDescriptor;
		bool _bColorFormat = true;
		DXGI_FORMAT _dxFormat = DXGI_FORMAT_UNKNOWN;
		D3D12_RESOURCE_STATES _rtState = D3D12_RESOURCE_STATE_COMMON;

	public:
		virtual void UploadToGpu() override { }
		D3D12RenderTarget(int32_t Width, int32_t Height, TextureFormat Format);
		D3D12RenderTarget(int32_t Width, int32_t Height, TextureFormat Format, ID3D12Resource *PriorResource);

		void TransitionTo(D3D12_RESOURCE_STATES InState);

		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle()
		{
			SE_ASSERT(_cpuDescriptor);
			return _cpuDescriptor->GetCPUDescriptorHandleForHeapStart();
		}

		void CreateCPUDescriptors();
		virtual ~D3D12RenderTarget() { }
	};
}