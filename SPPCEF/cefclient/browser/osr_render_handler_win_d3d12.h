// Copyright 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#pragma once

#include "cefclient/browser/osr_d3d12_win.h"
#include "cefclient/browser/osr_render_handler_win.h"
#include "cefclient/browser/osr_renderer_settings.h"

namespace client {

	class BrowserLayerD3D12 : public d3d12::Layer {
	public:
		explicit BrowserLayerD3D12(const std::shared_ptr<d3d12::Device>& device);

		void render() OVERRIDE;

		void on_paint(void* share_handle);

		// After calling on_paint() we can query the texture size.
		std::pair<uint32_t, uint32_t> texture_size() const;

	private:
		std::shared_ptr<d3d12::FrameBuffer> frame_buffer_;

		DISALLOW_COPY_AND_ASSIGN(BrowserLayerD3D12);
	};

	class PopupLayerD3D12 : public BrowserLayerD3D12 {
	public:
		explicit PopupLayerD3D12(const std::shared_ptr<d3d12::Device>& device);

		void set_bounds(const CefRect& bounds);

		bool contains(int x, int y) const { return bounds_.Contains(x, y); }
		int xoffset() const { return original_bounds_.x - bounds_.x; }
		int yoffset() const { return original_bounds_.y - bounds_.y; }

	private:
		CefRect original_bounds_;
		CefRect bounds_;

		DISALLOW_COPY_AND_ASSIGN(PopupLayerD3D12);
	};

	class OsrRenderHandlerWinD3D12 : public OsrRenderHandlerWin {
	public:
		OsrRenderHandlerWinD3D12(const OsrRendererSettings& settings, HWND hwnd);

		// Must be called immediately after object creation.
		// May fail if D3D11 cannot be initialized.
		bool Initialize(CefRefPtr<CefBrowser> browser, int width, int height);

		void SetSpin(float spinX, float spinY) OVERRIDE;
		void IncrementSpin(float spinDX, float spinDY) OVERRIDE;
		bool IsOverPopupWidget(int x, int y) const OVERRIDE;
		int GetPopupXOffset() const OVERRIDE;
		int GetPopupYOffset() const OVERRIDE;
		void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) OVERRIDE;
		void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) OVERRIDE;
		void OnPaint(CefRefPtr<CefBrowser> browser,
			CefRenderHandler::PaintElementType type,
			const CefRenderHandler::RectList& dirtyRects,
			const void* buffer,
			int width,
			int height) OVERRIDE;
		void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
			CefRenderHandler::PaintElementType type,
			const CefRenderHandler::RectList& dirtyRects,
			void* share_handle) OVERRIDE;

	private:
		void Render() OVERRIDE;

		uint64_t start_time_;
		std::shared_ptr<d3d12::Device> device_;
		std::shared_ptr<d3d12::SwapChain> swap_chain_;
		std::shared_ptr<d3d12::Composition> composition_;
		std::shared_ptr<BrowserLayerD3D12> browser_layer_;
		std::shared_ptr<PopupLayerD3D12> popup_layer_;

		DISALLOW_COPY_AND_ASSIGN(OsrRenderHandlerWinD3D12);
	};

}  // namespace client

