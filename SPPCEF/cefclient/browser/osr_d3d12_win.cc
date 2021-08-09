// Copyright 2018 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.
//
// Portions Copyright (c) 2018 Daktronics with the following MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

#include "cefclient/browser/osr_d3d12_win.h"

#include "d3dx12.h"
#include <directxmath.h>

#include "include/base/cef_logging.h"
#include "include/internal/cef_string.h"
#include "shared/browser/util_win.h"



namespace client {
	namespace d3d12 {

		struct SimpleVertex {
			DirectX::XMFLOAT3 pos;
			DirectX::XMFLOAT2 tex;
		};

		//TODO
		//SwapChain::SwapChain(IDXGISwapChain* swapchain,
		//	ID3D11RenderTargetView* rtv,
		//	ID3D11SamplerState* sampler,
		//	ID3D11BlendState* blender)
		//	: sampler_(to_com_ptr(sampler)),
		//	blender_(to_com_ptr(blender)),
		//	swapchain_(to_com_ptr(swapchain)),
		//	rtv_(to_com_ptr(rtv)),
		//	width_(0),
		//	height_(0) {}

		//void SwapChain::bind(ComPtr<ID3D12GraphicsCommandList> InCommandList)
		//{
		//	
		//}

		//void SwapChain::unbind()
		//{
		//}

		//void SwapChain::clear(float red, float green, float blue, float alpha) 
		//{
		//	ID3D11DeviceContext* d3d11_ctx = (ID3D11DeviceContext*)(*ctx_);
		//	CHECK(d3d11_ctx);

		//	FLOAT color[4] = { red, green, blue, alpha };
		//	d3d11_ctx->ClearRenderTargetView(rtv_.get(), color);
		//}

		//void SwapChain::present(int sync_interval) 
		//{
		//	swapchain_->Present(sync_interval, 0);
		//}

		//void SwapChain::resize(int width, int height) 
		//{
		//	if (width <= 0 || height <= 0 || width == width_ || height == height_)
		//	{
		//		return;
		//	}
		//	width_ = width;
		//	height_ = height;

		//	ID3D11DeviceContext* d3d11_ctx = (ID3D11DeviceContext*)(*ctx_);
		//	CHECK(d3d11_ctx);

		//	d3d11_ctx->OMSetRenderTargets(0, 0, 0);
		//	rtv_.reset();

		//	DXGI_SWAP_CHAIN_DESC desc;
		//	swapchain_->GetDesc(&desc);
		//	auto hr = swapchain_->ResizeBuffers(0, width, height, desc.BufferDesc.Format,
		//		desc.Flags);
		//	if (FAILED(hr)) {
		//		LOG(ERROR) << "d3d11: Failed to resize swapchain (" << width << "x"
		//			<< height << ")";
		//		return;
		//	}

		//	ID3D11Texture2D* buffer = nullptr;
		//	hr = swapchain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buffer);
		//	if (FAILED(hr)) {
		//		LOG(ERROR) << "d3d11: Failed to resize swapchain (" << width << "x"
		//			<< height << ")";
		//		return;
		//	}

		//	ID3D11Device* dev = nullptr;
		//	d3d11_ctx->GetDevice(&dev);
		//	if (dev) {
		//		D3D11_RENDER_TARGET_VIEW_DESC vdesc = {};
		//		vdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		//		vdesc.Texture2D.MipSlice = 0;
		//		vdesc.Format = desc.BufferDesc.Format;

		//		ID3D11RenderTargetView* view = nullptr;
		//		hr = dev->CreateRenderTargetView(buffer, &vdesc, &view);
		//		if (SUCCEEDED(hr)) {
		//			rtv_ = to_com_ptr(view);
		//			d3d11_ctx->OMSetRenderTargets(1, &view, nullptr);
		//		}
		//		dev->Release();
		//	}
		//	buffer->Release();

		//	D3D11_VIEWPORT vp;
		//	vp.Width = static_cast<float>(width);
		//	vp.Height = static_cast<float>(height);
		//	vp.MinDepth = D3D11_MIN_DEPTH;
		//	vp.MaxDepth = D3D11_MAX_DEPTH;
		//	vp.TopLeftX = 0;
		//	vp.TopLeftY = 0;
		//	d3d11_ctx->RSSetViewports(1, &vp);
		//}
				
		void Effect::bind(ComPtr<ID3D12GraphicsCommandList> InCommandList)
		{
			//ctx_ = ctx;
			//auto commandList = ctx->_commandlist;

			//commandList->IASetInputLayout(layout_.get());
			//commandList->VSSetShader(vsh_.get(), nullptr, 0);
			//commandList->PSSetShader(psh_.get(), nullptr, 0);
		}

		void Effect::unbind() {}

		void Geometry::bind(ComPtr<ID3D12GraphicsCommandList> InCommandList)
		{
			//ctx_ = ctx;
			//ID3D11DeviceContext* d3d11_ctx = (ID3D11DeviceContext*)(*ctx_);

			//// TODO: Handle offset.
			//uint32_t offset = 0;

			//ID3D11Buffer* buffers[1] = {buffer_.get()};
			//d3d11_ctx->IASetVertexBuffers(0, 1, buffers, &stride_, &offset);
			//d3d11_ctx->IASetPrimitiveTopology(primitive_);

			//InCommandList->IASetVertexBuffers(0, 1, _meshData->vertexBuffer->GetAs<D3D12VertexBuffer>().GetView());
			
		}

		void Geometry::unbind() {}

		void Geometry::draw()
		{
			//ID3D11DeviceContext* d3d11_ctx = (ID3D11DeviceContext*)(*ctx_);
			//CHECK(d3d11_ctx);

			//// TODO: Handle offset.
			//d3d11_ctx->Draw(vertices_, 0);

			//cmdList->DrawInstanced(vertices_, 1, 0, 0);
		}

		

		uint32_t Texture2D::width() const {
			return _description.Width;
		}

		uint32_t Texture2D::height() const {
			return _description.Height;
		}

		DXGI_FORMAT Texture2D::format() const {
			return _description.Format;
		}		
		void Texture2D::bind(ComPtr<ID3D12GraphicsCommandList> InCommandList)
		{			
		}

		void Texture2D::unbind()
		{
		}

		void GetHardwareAdapter(
			IDXGIFactory1* pFactory,
			IDXGIAdapter1** ppAdapter,
			bool requestHighPerformanceAdapter)
		{
			*ppAdapter = nullptr;

			ComPtr<IDXGIAdapter1> adapter;
			ComPtr<IDXGIFactory6> factory6;
			if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
			{
				for (
					UINT adapterIndex = 0;
					DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
						adapterIndex,
						requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
						IID_PPV_ARGS(&adapter));
					++adapterIndex)
				{
					DXGI_ADAPTER_DESC1 desc;
					adapter->GetDesc1(&desc);

					if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					{
						// Don't select the Basic Render Driver adapter.
						// If you want a software adapter, pass in "/warp" on the command line.
						continue;
					}

					// Check to see whether the adapter supports Direct3D 12, but don't create the
					// actual device yet.
					if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
					{
						break;
					}
				}
			}
			else
			{
				for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
				{
					DXGI_ADAPTER_DESC1 desc;
					adapter->GetDesc1(&desc);

					if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					{
						// Don't select the Basic Render Driver adapter.
						// If you want a software adapter, pass in "/warp" on the command line.
						continue;
					}

					// Check to see whether the adapter supports Direct3D 12, but don't create the
					// actual device yet.
					if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
					{
						break;
					}
				}
			}

			*ppAdapter = adapter.Detach();
		}

		// static
		std::shared_ptr<Device> Device::create() 
		{
			UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
			// Enable the debug layer (requires the Graphics Tools "optional feature").
			// NOTE: Enabling the debug layer after device creation will invalidate the active device.
			{
				ComPtr<ID3D12Debug> debugController;
				if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
				{
					debugController->EnableDebugLayer();

					// Enable additional debug layers.
					dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
				}
			}
#endif

			ComPtr<IDXGIFactory4> factory;
			CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));

			ComPtr<IDXGIAdapter1> hardwareAdapter;
			GetHardwareAdapter(factory.Get(), &hardwareAdapter, true);

			ComPtr<ID3D12Device> m_device;
			D3D12CreateDevice(
				hardwareAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&m_device)
			);			

			return std::make_shared<Device>(m_device, factory);
		}

		void Device::SignalAndWaitForGPU()
		{
			// Signal and increment the fence value.
			const UINT64 fence = _fenceValue;
			_commandQueue->Signal(_fence.Get(), fence);
			_fenceValue++;

			// Wait until the previous frame is finished.
			if (_fence->GetCompletedValue() < fence)
			{
				_fence->SetEventOnCompletion(fence, _fenceEvent);
				WaitForSingleObject(_fenceEvent, INFINITE);
			}
		}

		void Device::CreateSwapChain(HWND window)
		{
			HRESULT hr;

			// Describe and create the command queue.
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			//queue
			_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue));
			//allocator
			_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator));
			//commandlist
			_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.Get(), nullptr, IID_PPV_ARGS(&_commandList));
			//// Command lists are created in the recording state, but there is nothing
			//// to record yet. The main loop expects it to be closed, so close it now.
			_commandList->Close();

			int width = -1;
			int height = -1;
			// Default size to the window size unless specified.
			RECT rc_bounds;
			GetClientRect(window, &rc_bounds);
			if (width <= 0) {
				width = rc_bounds.right - rc_bounds.left;
			}
			if (height <= 0) {
				height = rc_bounds.bottom - rc_bounds.top;
			}

			// Describe and create the swap chain.
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.BufferCount = 1;
			swapChainDesc.Width = width;
			swapChainDesc.Height = height;
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.SampleDesc.Count = 1;

			ComPtr<IDXGISwapChain1> swapChain;
			_factory->CreateSwapChainForHwnd(
				_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
				window,
				&swapChainDesc,
				nullptr,
				nullptr,
				&swapChain
			);

			// This sample does not support fullscreen transitions.
			_factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);

			// just set it
			swapChain.As(&_swapChain);

			// Create synchronization objects and wait until assets have been uploaded to the GPU.
			{
				_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
				_fenceValue = 1;

				// Create an event handle to use for frame synchronization.
				_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

				// Wait for the command list to execute; we are reusing the same command 
				// list in our main loop but for now, we just want to wait for setup to 
				// complete before continuing.
				SignalAndWaitForGPU();
			}
		}

		std::shared_ptr<Geometry> Device::create_quad(float x,
			float y,
			float width,
			float height,
			bool flip) 
		{
			x = (x * 2.0f) - 1.0f;
			y = 1.0f - (y * 2.0f);
			width = width * 2.0f;
			height = height * 2.0f;
			float z = 1.0f;

			SimpleVertex vertices[] = {
				{DirectX::XMFLOAT3(x, y, z), DirectX::XMFLOAT2(0.0f, 0.0f)},
				{DirectX::XMFLOAT3(x + width, y, z), DirectX::XMFLOAT2(1.0f, 0.0f)},
				{DirectX::XMFLOAT3(x, y - height, z), DirectX::XMFLOAT2(0.0f, 1.0f)},
				{DirectX::XMFLOAT3(x + width, y - height, z),
				 DirectX::XMFLOAT2(1.0f, 1.0f)} };

			if (flip) {
				DirectX::XMFLOAT2 tmp(vertices[2].tex);
				vertices[2].tex = vertices[0].tex;
				vertices[0].tex = tmp;

				tmp = vertices[3].tex;
				vertices[3].tex = vertices[1].tex;
				vertices[1].tex = tmp;
			}

			const UINT vertexBufferSize = sizeof(vertices);

			ComPtr<ID3D12Resource> vertexBuffer;

			// Note: using upload heaps to transfer static data like vert buffers is not 
			// recommended. Every time the GPU needs it, the upload heap will be marshalled 
			// over. Please read up on Default Heap usage. An upload heap is used here for 
			// code simplicity and because there are very few verts to actually transfer.
			_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&vertexBuffer));

			// Copy the triangle data to the vertex buffer.
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
			vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
			memcpy(pVertexDataBegin, vertices, sizeof(vertices));
			vertexBuffer->Unmap(0, nullptr);

			return std::make_shared<Geometry>( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4, static_cast<uint32_t>(sizeof(SimpleVertex)), vertexBuffer);
		}		

		std::shared_ptr<Texture2D> Device::create_texture(int width,
			int height,
			DXGI_FORMAT format,
			const void* data,
			size_t row_stride) 
		{
			D3D12_RESOURCE_DESC td;

			td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			td.Width = width;
			td.Height = height;
			td.DepthOrArraySize = 1;
			td.MipLevels = 1;
			td.Format = format;
			td.SampleDesc.Count = 1;
			td.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			ComPtr<ID3D12Resource> textureResource;
			if (FAILED(_device->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&td,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(&textureResource))))
			{
				throw std::runtime_error("Failed to create 2D texture");
			}

			std::vector<D3D12_SUBRESOURCE_DATA> subresources;
			D3D12_SUBRESOURCE_DATA textureData = {};
			//TODO CHECK THESE
			textureData.pData = data;
			textureData.RowPitch = row_stride;
			textureData.SlicePitch = textureData.RowPitch * height;
			subresources.push_back(textureData);

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource.Get(), 0, subresources.size());

			ComPtr<ID3D12Resource> textureUpload;

			auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto upBufSz = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
			// Create the GPU upload buffer.
			_device->CreateCommittedResource(
				&heapUpload,
				D3D12_HEAP_FLAG_NONE,
				&upBufSz,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&textureUpload));

			///TODO WHY?
			//UpdateSubresources(_commandList, textureResource.Get(), textureUpload.Get(), 0, 0, subresources.size(), subresources.data());
			auto textureTransition = CD3DX12_RESOURCE_BARRIER::Transition(textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			_commandList->ResourceBarrier(1, &textureTransition);

			return std::make_shared<Texture2D>(textureResource, td);
		}

		std::shared_ptr<ID3DBlob> Device::compile_shader(const std::string& source_code,
			const std::string& entry_point,
			const std::string& model)
		{
			//TODO
			return nullptr;
		}

		std::shared_ptr<Effect> Device::create_default_effect() {
			const auto vsh =
R"--(struct VS_INPUT
{
	float4 pos : POSITION;
	float2 tex : TEXCOORD0;};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.pos = input.pos;
	output.tex = input.tex;
	return output;
})--";

			const auto psh =
R"--(Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_Target
{
	return tex0.Sample(samp0, input.tex);
})--";

			return create_effect(vsh, "main", "vs_4_0", psh, "main", "ps_4_0");
		}

		std::shared_ptr<Effect> Device::create_effect(const std::string& vertex_code,
			const std::string& vertex_entry,
			const std::string& vertex_model,
			const std::string& pixel_code,
			const std::string& pixel_entry,
			const std::string& pixel_model) 
		{
			//const auto vs_blob = compile_shader(vertex_code, vertex_entry, vertex_model);

			//ID3D11VertexShader* vshdr = nullptr;
			//ID3D11InputLayout* layout = nullptr;

			//if (vs_blob) {
			//	device_->CreateVertexShader(vs_blob->GetBufferPointer(),
			//		vs_blob->GetBufferSize(), nullptr, &vshdr);

			//	D3D11_INPUT_ELEMENT_DESC layout_desc[] = {
			//		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
			//		 D3D11_INPUT_PER_VERTEX_DATA, 0},
			//		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
			//		 D3D11_INPUT_PER_VERTEX_DATA, 0},
			//	};

			//	UINT elements = ARRAYSIZE(layout_desc);

			//	// Create the input layout.
			//	device_->CreateInputLayout(layout_desc, elements,
			//		vs_blob->GetBufferPointer(),
			//		vs_blob->GetBufferSize(), &layout);
			//}

			//const auto ps_blob = compile_shader(pixel_code, pixel_entry, pixel_model);
			//ID3D11PixelShader* pshdr = nullptr;
			//if (ps_blob) {
			//	device_->CreatePixelShader(ps_blob->GetBufferPointer(),
			//		ps_blob->GetBufferSize(), nullptr, &pshdr);
			//}

			//return std::make_shared<Effect>(vshdr, pshdr, layout);
			//TODO
			return nullptr;
		}

		Layer::Layer(const std::shared_ptr<Device>& device, bool flip)
			: device_(device), flip_(flip) {
			bounds_.x = bounds_.y = bounds_.width = bounds_.height = 0.0f;
		}

		Layer::~Layer() {}

		void Layer::attach(const std::shared_ptr<Composition>& parent) {
			composition_ = parent;
		}

		std::shared_ptr<Composition> Layer::composition() const {
			return composition_.lock();
		}

		Rect Layer::bounds() const {
			return bounds_;
		}

		void Layer::move(float x, float y, float width, float height) {
			bounds_.x = x;
			bounds_.y = y;
			bounds_.width = width;
			bounds_.height = height;

			// It's not efficient to create the quad everytime we move, but for now we're
			// just trying to get something on-screen.
			geometry_.reset();
		}

		void Layer::tick(double) {
			// Nothing to update in the base class.
		}

		void Layer::render_texture(const std::shared_ptr<Texture2D>& texture) 
		{
			if (!geometry_) {
				geometry_ = device_->create_quad(bounds_.x, bounds_.y, bounds_.width,
					bounds_.height, flip_);
			}

			if (geometry_ && texture) {
				// We need a shader.
				if (!effect_) {
					effect_ = device_->create_default_effect();
				}

				//TODO FIX BINDINGS
				// Bind our states/resource to the pipeline.
				//ScopedBinder<Geometry> quad_binder(ctx, geometry_);
				//ScopedBinder<Effect> fx_binder(ctx, effect_);
				//ScopedBinder<Texture2D> tex_binder(ctx, texture);

				// Draw the quad.
				geometry_->draw();
			}
		}

		Composition::Composition(const std::shared_ptr<Device>& device,
			int width,
			int height)
			: width_(width), height_(height), vsync_(true), device_(device) {
			fps_ = 0.0;
			time_ = 0.0;
			frame_ = 0;
			fps_start_ = GetTimeNow();
		}

		bool Composition::is_vsync() const {
			return vsync_;
		}

		double Composition::time() const {
			return time_;
		}

		double Composition::fps() const {
			return fps_;
		}

		void Composition::add_layer(const std::shared_ptr<Layer>& layer) {
			if (layer) {
				layers_.push_back(layer);

				// Attach ourselves as the parent.
				layer->attach(shared_from_this());
			}
		}

		bool Composition::remove_layer(const std::shared_ptr<Layer>& layer) {
			size_t match = 0;
			if (layer) {
				for (auto i = layers_.begin(); i != layers_.end();) {
					if ((*i).get() == layer.get()) {
						i = layers_.erase(i);
						match++;
					}
					else {
						i++;
					}
				}
			}
			return (match > 0);
		}

		void Composition::resize(bool vsync, int width, int height) {
			vsync_ = vsync;
			width_ = width;
			height_ = height;
		}

		void Composition::tick(double t) {
			time_ = t;
			for (const auto& layer : layers_) {
				layer->tick(t);
			}
		}

		void Composition::render() {
			// Use painter's algorithm and render our layers in order (not doing any dept
			// or 3D here).
			for (const auto& layer : layers_) {
				layer->render();
			}

			frame_++;
			const auto now = GetTimeNow();
			if ((now - fps_start_) > 1000000) {
				fps_ = frame_ / double((now - fps_start_) / 1000000.0);
				frame_ = 0;
				fps_start_ = now;
			}
		}

		FrameBuffer::FrameBuffer(const std::shared_ptr<Device>& device)
			: device_(device) {}

		void FrameBuffer::on_paint(void* shared_handle) {
			
		}

	}  // namespace d3d11
}  // namespace client
