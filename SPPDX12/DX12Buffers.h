// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once
#include "DX12Device.h"

namespace SPP
{	
	class D3D12Buffer : public GPUBuffer
	{
	protected:
		ComPtr<ID3D12Resource> _buffer;
		//ComPtr<ID3D12Resource> _heapUpload;
		D3D12_RESOURCE_STATES _currentState;

	public:
		D3D12Buffer(std::shared_ptr< ArrayResource > InCpuData);

		virtual ~D3D12Buffer();
		
		virtual void UploadToGpu() override;
		virtual void UpdateDirtyRegion(uint32_t Idx, uint32_t Count) override;

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress()
		{		
			SE_ASSERT(_buffer);
			return _buffer->GetGPUVirtualAddress();
		}
		ID3D12Resource* GetResource() const
		{
			SE_ASSERT(_buffer);
			return _buffer.Get();
		}
	};

	class D3D12IndexBuffer : public D3D12Buffer
	{
	protected:
		D3D12_INDEX_BUFFER_VIEW _view = { 0 };
		uint32_t  _numElements = 0;

	public:
		D3D12IndexBuffer(std::shared_ptr< ArrayResource > InCpuData);
		virtual ~D3D12IndexBuffer();
		virtual void UploadToGpu() override;
		D3D12_INDEX_BUFFER_VIEW* GetView();
		uint32_t GetCachedElementCount() const;
		void ConfigureView();
	};

	class D3D12VertexBuffer : public D3D12Buffer
	{
	protected:
		D3D12_VERTEX_BUFFER_VIEW _view = { 0 };

	public:
		D3D12VertexBuffer(std::shared_ptr< ArrayResource > InCpuData);
		virtual ~D3D12VertexBuffer();
		virtual void UploadToGpu() override;

		D3D12_VERTEX_BUFFER_VIEW* GetView();

		void ConfigureView();
	};

	class D3D12GlobalBuffer : public GPUBuffer
	{
	private:
		ComPtr<ID3D12Resource> _heapUpload;
		ComPtr<ID3D12Heap> _heap;
		uint32_t _numberOfTiles = 0;

	public:
		D3D12GlobalBuffer(std::shared_ptr< ArrayResource > InCpuData);
		ID3D12Resource* GetUploadHeap()
		{
			return _heapUpload.Get();
		}
		ID3D12Heap *GetHeap()
		{
			return _heap.Get();
		}
		uint32_t TileCount() const
		{
			return _numberOfTiles;
		}

		ID3D12Resource* GetResource() const;

		virtual ~D3D12GlobalBuffer();
		virtual void UploadToGpu() override;
	};

	class D3D12InputLayout : public GPUInputLayout
	{
	protected:
		std::vector<D3D12_INPUT_ELEMENT_DESC> _layout;
		std::vector< InputLayoutElement> _storedList;

	public:
		static DXGI_FORMAT LayouttoDXGIFormat(const InputLayoutElementType& InType);
		size_t GetCount();
		D3D12_INPUT_ELEMENT_DESC* GetData();
		virtual void UploadToGpu() override;
		void InitializeLayout(const std::vector< InputLayoutElement>& eleList);
	};

	
}