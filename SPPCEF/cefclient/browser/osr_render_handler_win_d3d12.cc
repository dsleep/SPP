// Copyright 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/browser/osr_render_handler_win_d3d12.h"

#include "include/base/cef_bind.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "shared/browser/util_win.h"

namespace client {

	BrowserLayerD3D12::BrowserLayerD3D12(const std::shared_ptr<d3d12::Device>& device)
		: d3d12::Layer(device, true /* flip */) {
		frame_buffer_ = std::make_shared<d3d12::FrameBuffer>(device_);
	}

	void BrowserLayerD3D12::render() {
		// Use the base class method to draw our texture.
		render_texture(frame_buffer_->texture());
	}

	void BrowserLayerD3D12::on_paint(void* share_handle) {
		frame_buffer_->on_paint(share_handle);
	}

	std::pair<uint32_t, uint32_t> BrowserLayerD3D12::texture_size() const {
		const auto texture = frame_buffer_->texture();
		return std::make_pair(texture->width(), texture->height());
	}

	PopupLayerD3D12::PopupLayerD3D12(const std::shared_ptr<d3d12::Device>& device)
		: BrowserLayerD3D12(device) {}

	void PopupLayerD3D12::set_bounds(const CefRect& bounds) {
		const auto comp = composition();
		if (!comp)
			return;

		const auto outer_width = comp->width();
		const auto outer_height = comp->height();
		if (outer_width == 0 || outer_height == 0)
			return;

		original_bounds_ = bounds;
		bounds_ = bounds;

		// If x or y are negative, move them to 0.
		if (bounds_.x < 0)
			bounds_.x = 0;
		if (bounds_.y < 0)
			bounds_.y = 0;
		// If popup goes outside the view, try to reposition origin
		if (bounds_.x + bounds_.width > outer_width)
			bounds_.x = outer_width - bounds_.width;
		if (bounds_.y + bounds_.height > outer_height)
			bounds_.y = outer_height - bounds_.height;
		// If x or y became negative, move them to 0 again.
		if (bounds_.x < 0)
			bounds_.x = 0;
		if (bounds_.y < 0)
			bounds_.y = 0;

		const auto x = bounds_.x / float(outer_width);
		const auto y = bounds_.y / float(outer_height);
		const auto w = bounds_.width / float(outer_width);
		const auto h = bounds_.height / float(outer_height);
		move(x, y, w, h);
	}

	OsrRenderHandlerWinD3D12::OsrRenderHandlerWinD3D12(
		const OsrRendererSettings& settings,
		HWND hwnd)
		: OsrRenderHandlerWin(settings, hwnd), start_time_(0) {}

	bool OsrRenderHandlerWinD3D12::Initialize(CefRefPtr<CefBrowser> browser,
		int width,
		int height) 
	{
		CEF_REQUIRE_UI_THREAD();

		// Create a D3D11 device instance.
		device_ = d3d12::Device::create();
		DCHECK(device_);
		if (!device_)
			return false;

		// Create a D3D11 swapchain for the window.
		device_->CreateSwapChain(hwnd());
		
		// Create the browser layer.
		browser_layer_ = std::make_shared<BrowserLayerD3D12>(device_);

		// Set up the composition.
		composition_ = std::make_shared<d3d12::Composition>(device_, width, height);
		composition_->add_layer(browser_layer_);

		// Size to the whole composition.
		browser_layer_->move(0.0f, 0.0f, 1.0f, 1.0f);

		start_time_ = GetTimeNow();

		SetBrowser(browser);
		return true;
	}

	void OsrRenderHandlerWinD3D12::SetSpin(float spinX, float spinY) {
		CEF_REQUIRE_UI_THREAD();
		// Spin support is not implemented.
	}

	void OsrRenderHandlerWinD3D12::IncrementSpin(float spinDX, float spinDY) {
		CEF_REQUIRE_UI_THREAD();
		// Spin support is not implemented.
	}

	bool OsrRenderHandlerWinD3D12::IsOverPopupWidget(int x, int y) const {
		CEF_REQUIRE_UI_THREAD();
		return popup_layer_ && popup_layer_->contains(x, y);
	}

	int OsrRenderHandlerWinD3D12::GetPopupXOffset() const {
		CEF_REQUIRE_UI_THREAD();
		if (popup_layer_)
			return popup_layer_->xoffset();
		return 0;
	}

	int OsrRenderHandlerWinD3D12::GetPopupYOffset() const {
		CEF_REQUIRE_UI_THREAD();
		if (popup_layer_)
			return popup_layer_->yoffset();
		return 0;
	}

	void OsrRenderHandlerWinD3D12::OnPopupShow(CefRefPtr<CefBrowser> browser,
		bool show) {
		CEF_REQUIRE_UI_THREAD();

		if (show) {
			DCHECK(!popup_layer_);

			// Create a new layer.
			popup_layer_ = std::make_shared<PopupLayerD3D12>(device_);
			composition_->add_layer(popup_layer_);
		}
		else {
			DCHECK(popup_layer_);

			composition_->remove_layer(popup_layer_);
			popup_layer_ = nullptr;

			Render();
		}
	}

	void OsrRenderHandlerWinD3D12::OnPopupSize(CefRefPtr<CefBrowser> browser,
		const CefRect& rect) {
		CEF_REQUIRE_UI_THREAD();
		popup_layer_->set_bounds(rect);
	}

	void OsrRenderHandlerWinD3D12::OnPaint(
		CefRefPtr<CefBrowser> browser,
		CefRenderHandler::PaintElementType type,
		const CefRenderHandler::RectList& dirtyRects,
		const void* buffer,
		int width,
		int height)
	{

		//if (!initialized_)
		//	Initialize();

		//if (IsTransparent()) {
		//	// Enable alpha blending.
		//	glEnable(GL_BLEND);
		//	VERIFY_NO_ERROR;
		//}

		//// Enable 2D textures.
		//glEnable(GL_TEXTURE_2D);
		//VERIFY_NO_ERROR;

		//DCHECK_NE(texture_id_, 0U);
		//glBindTexture(GL_TEXTURE_2D, texture_id_);
		//VERIFY_NO_ERROR;

		//if (type == PET_VIEW) {
		//	int old_width = view_width_;
		//	int old_height = view_height_;

		//	view_width_ = width;
		//	view_height_ = height;

		//	if (settings_.show_update_rect)
		//		update_rect_ = dirtyRects[0];

		//	glPixelStorei(GL_UNPACK_ROW_LENGTH, view_width_);
		//	VERIFY_NO_ERROR;

		//	if (old_width != view_width_ || old_height != view_height_ ||
		//		(dirtyRects.size() == 1 &&
		//			dirtyRects[0] == CefRect(0, 0, view_width_, view_height_))) {
		//		// Update/resize the whole texture.
		//		glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
		//		VERIFY_NO_ERROR;
		//		glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
		//		VERIFY_NO_ERROR;
		//		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, view_width_, view_height_, 0,
		//			GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
		//		VERIFY_NO_ERROR;
		//	}
		//	else {
		//		// Update just the dirty rectangles.
		//		CefRenderHandler::RectList::const_iterator i = dirtyRects.begin();
		//		for (; i != dirtyRects.end(); ++i) {
		//			const CefRect& rect = *i;
		//			DCHECK(rect.x + rect.width <= view_width_);
		//			DCHECK(rect.y + rect.height <= view_height_);
		//			glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect.x);
		//			VERIFY_NO_ERROR;
		//			glPixelStorei(GL_UNPACK_SKIP_ROWS, rect.y);
		//			VERIFY_NO_ERROR;
		//			glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.width,
		//				rect.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
		//				buffer);
		//			VERIFY_NO_ERROR;
		//		}
		//	}
		//}
		//else if (type == PET_POPUP && popup_rect_.width > 0 &&
		//	popup_rect_.height > 0) {
		//	int skip_pixels = 0, x = popup_rect_.x;
		//	int skip_rows = 0, y = popup_rect_.y;
		//	int w = width;
		//	int h = height;

		//	// Adjust the popup to fit inside the view.
		//	if (x < 0) {
		//		skip_pixels = -x;
		//		x = 0;
		//	}
		//	if (y < 0) {
		//		skip_rows = -y;
		//		y = 0;
		//	}
		//	if (x + w > view_width_)
		//		w -= x + w - view_width_;
		//	if (y + h > view_height_)
		//		h -= y + h - view_height_;

		//	// Update the popup rectangle.
		//	glPixelStorei(GL_UNPACK_ROW_LENGTH, width);
		//	VERIFY_NO_ERROR;
		//	glPixelStorei(GL_UNPACK_SKIP_PIXELS, skip_pixels);
		//	VERIFY_NO_ERROR;
		//	glPixelStorei(GL_UNPACK_SKIP_ROWS, skip_rows);
		//	VERIFY_NO_ERROR;
		//	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_BGRA,
		//		GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
		//	VERIFY_NO_ERROR;
		//}

		//// Disable 2D textures.
		//glDisable(GL_TEXTURE_2D);
		//VERIFY_NO_ERROR;

		//if (IsTransparent()) {
		//	// Disable alpha blending.
		//	glDisable(GL_BLEND);
		//	VERIFY_NO_ERROR;
		//}
	}

	void OsrRenderHandlerWinD3D12::OnAcceleratedPaint(
		CefRefPtr<CefBrowser> browser,
		CefRenderHandler::PaintElementType type,
		const CefRenderHandler::RectList& dirtyRects,
		void* share_handle) {
		CEF_REQUIRE_UI_THREAD();

		if (type == PET_POPUP) {
			popup_layer_->on_paint(share_handle);
		}
		else {
			browser_layer_->on_paint(share_handle);
		}

		Render();
	}

	void OsrRenderHandlerWinD3D12::Render()
	{
		//TODO
		// Update composition + layers based on time.
		//const auto t = (GetTimeNow() - start_time_) / 1000000.0;
		//composition_->tick(t);

		//auto ctx = device_->immedidate_context();
		//swap_chain_->bind(ctx);

		//const auto texture_size = browser_layer_->texture_size();

		//// Resize the composition and swap chain to match the texture if necessary.
		//composition_->resize(!send_begin_frame(), texture_size.first,
		//	texture_size.second);
		//swap_chain_->resize(texture_size.first, texture_size.second);

		//// Clear the render target.
		//swap_chain_->clear(0.0f, 0.0f, 1.0f, 1.0f);

		//// Render the scene.
		//composition_->render(ctx);

		//// Present to window.
		//swap_chain_->present(send_begin_frame() ? 0 : 1);
	}

}  // namespace client
