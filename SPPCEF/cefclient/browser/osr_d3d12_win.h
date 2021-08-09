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

#pragma once


#include <d3d12.h>
#include "dxc/inc/dxcapi.h"
#include <dxgi1_6.h>
#include <wrl.h>

#include <memory>
#include <string>
#include <vector>

#include "include/base/cef_macros.h"

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

namespace client 
{
	namespace d3d12
	{
		class Composition;
		class Effect;
		class Geometry;
		class SwapChain;
		class Texture2D;

		// Basic rect for floats.
		struct Rect {
			float x;
			float y;
			float width;
			float height;
		};

		// Encapsulate a D3D11 Device object.
		class Device
		{
		public:
			Device(ComPtr<ID3D12Device> InDevice, ComPtr<IDXGIFactory4> InFactory) : _device(InDevice), _factory(InFactory) {}

			static std::shared_ptr<Device> create();

			operator ID3D12Device* () { return _device.Get(); }
			ID3D12GraphicsCommandList* GetCommmandList() { return _commandList.Get(); }


			std::shared_ptr<Geometry> create_quad(float x,
				float y,
				float width,
				float height,
				bool flip = false);

			std::shared_ptr<Texture2D> create_texture(int width,
				int height,
				DXGI_FORMAT format,
				const void* data,
				size_t row_stride);

			std::shared_ptr<Texture2D> open_shared_texture(void*);

			// Create some basic shaders so we can draw a textured-quad.
			std::shared_ptr<Effect> create_default_effect();

			std::shared_ptr<Effect> create_effect(const std::string& vertex_code,
				const std::string& vertex_entry,
				const std::string& vertex_model,
				const std::string& pixel_code,
				const std::string& pixel_entry,
				const std::string& pixel_model);


			void SignalAndWaitForGPU();
			void CreateSwapChain(HWND window);

		private:
			std::shared_ptr<ID3DBlob> compile_shader(const std::string& source_code,
				const std::string& entry_point,
				const std::string& model);

			

			ComPtr<ID3D12Device> _device;
			ComPtr<IDXGIFactory4> _factory;

			ComPtr<ID3D12CommandQueue> _commandQueue;
			ComPtr<ID3D12CommandAllocator> _commandAllocator;
			ComPtr<ID3D12GraphicsCommandList> _commandList;
			
			ComPtr<IDXGISwapChain3> _swapChain;
			ComPtr<ID3D12Fence> _fence;

			HANDLE _fenceEvent;
			UINT64 _fenceValue;

			DISALLOW_COPY_AND_ASSIGN(Device);
		};

		
		class Texture2D
		{
		public:
			Texture2D(ComPtr < ID3D12Resource > InTex, D3D12_RESOURCE_DESC InDesc) : _texture(InTex), _description(InDesc)
			{

			}

			void bind(ComPtr<ID3D12GraphicsCommandList> InCommandList);
			void unbind();

			uint32_t width() const;
			uint32_t height() const;
			DXGI_FORMAT format() const;

		private:
			D3D12_RESOURCE_DESC _description;
			ComPtr<ID3D12Resource> _texture;

			DISALLOW_COPY_AND_ASSIGN(Texture2D);
		};

			

		class Effect 
		{
		public:
			//TODO set layout to move &&
			Effect(ComPtr<IDxcBlob> InVS, ComPtr<IDxcBlob> InPS, const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout) : _vs(InVS), _ps(InPS), _layout(layout)
			{				
			}

			void bind(ComPtr<ID3D12GraphicsCommandList> InCommandList);
			void unbind();

		private:
			ComPtr<IDxcBlob> _vs;
			ComPtr<IDxcBlob> _ps;
			std::vector<D3D12_INPUT_ELEMENT_DESC> _layout;

			DISALLOW_COPY_AND_ASSIGN(Effect);
		};

		class Geometry
		{
		public:
			Geometry(D3D_PRIMITIVE_TOPOLOGY primitive,
				uint32_t vertices,
				uint32_t stride,
				ComPtr<ID3D12Resource> InBuffer) : _primitive(primitive), _vertices(vertices), _stride(stride), _buffer(InBuffer)
			{
				_bufferView.BufferLocation = _buffer->GetGPUVirtualAddress();
				_bufferView.StrideInBytes = stride;
				_bufferView.SizeInBytes = vertices * stride;
			}

			void bind(ComPtr<ID3D12GraphicsCommandList> InCommandList);
			void unbind();
			void draw();

		private:
			D3D_PRIMITIVE_TOPOLOGY _primitive;
			uint32_t _vertices;
			uint32_t _stride;
			ComPtr<ID3D12Resource> _buffer;
			D3D12_VERTEX_BUFFER_VIEW _bufferView;

			DISALLOW_COPY_AND_ASSIGN(Geometry);
		};

		// Abstraction for a 2D layer within a composition.
		class Layer
		{
		public:
			Layer(const std::shared_ptr<Device>& device, bool flip);
			virtual ~Layer();

			void attach(const std::shared_ptr<Composition>&);

			// Uses normalized 0-1.0 coordinates.
			virtual void move(float x, float y, float width, float height);

			virtual void tick(double t);
			virtual void render() = 0;

			Rect bounds() const;

			std::shared_ptr<Composition> composition() const;

		protected:
			// Helper method for derived classes to draw a textured-quad.
			void render_texture(const std::shared_ptr<Texture2D>& texture);

			const std::shared_ptr<Device> device_;
			const bool flip_;

			Rect bounds_;
			std::shared_ptr<Geometry> geometry_;
			std::shared_ptr<Effect> effect_;

		private:
			std::weak_ptr<Composition> composition_;

			DISALLOW_COPY_AND_ASSIGN(Layer);
		};

		// A collection of layers. Will render 1-N layers to a D3D11 device.
		class Composition : public std::enable_shared_from_this<Composition> {
		public:
			Composition(const std::shared_ptr<Device>& device,
				int width = 0,
				int height = 0);

			int width() const { return width_; }
			int height() const { return height_; }

			double fps() const;
			double time() const;

			bool is_vsync() const;

			void tick(double);
			void render();

			void add_layer(const std::shared_ptr<Layer>& layer);
			bool remove_layer(const std::shared_ptr<Layer>& layer);
			void resize(bool vsync, int width, int height);

		private:
			int width_;
			int height_;
			uint32_t frame_;
			int64_t fps_start_;
			double fps_;
			double time_;
			bool vsync_;

			const std::shared_ptr<Device> device_;
			std::vector<std::shared_ptr<Layer>> layers_;

			DISALLOW_COPY_AND_ASSIGN(Composition);
		};

		class FrameBuffer
		{
		public:
			explicit FrameBuffer(const std::shared_ptr<Device>& device);

			// Called in response to CEF's OnAcceleratedPaint notification.
			void on_paint(void* shared_handle);

			// Returns what should be considered the front buffer.
			std::shared_ptr<Texture2D> texture() const { return shared_buffer_; }

		private:
			const std::shared_ptr<Device> device_;
			std::shared_ptr<Texture2D> shared_buffer_;

			DISALLOW_COPY_AND_ASSIGN(FrameBuffer);
		};

	}  // namespace d3d12
}  // namespace client

