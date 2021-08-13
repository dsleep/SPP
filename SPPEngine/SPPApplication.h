// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPEngine.h"
#include <functional>

namespace SPP
{

	struct SPP_ENGINE_API ApplicationEvents
	{
		std::function<void(void)> _msgLoop;
		std::function<void(void)> _windowCreated;
		std::function<void(void)> _windowClosed;
		std::function<void(void)> _windowMoved;
		std::function<void(void)> _onSuspend;
		std::function<void(void)> _onResume;
		std::function<void(int32_t, int32_t)> _onSizeChanged;
	};

	enum class EMouseButton
	{
		Left,
		Middle,
		Right
	};

	struct SPP_ENGINE_API InputEvents
	{
		std::function<void(uint8_t)> keyDown;
		std::function<void(uint8_t)> keyUp;
		std::function<void(int32_t, int32_t, EMouseButton)> mouseDown;
		std::function<void(int32_t, int32_t, EMouseButton)> mouseUp;
		std::function<void(int32_t, int32_t)> mouseMove;
	};

	enum class AppFlags
	{
		SupportOpenGL,
		None
	};

	class SPP_ENGINE_API ApplicationWindow : public ApplicationEvents, public InputEvents
	{
	protected:
		int32_t _width = -1;
		int32_t _height = -1;

	public:
		ApplicationWindow() {}

		void GetWidthHeight(int32_t& oWidth, int32_t& oHeight)
		{
			oWidth = _width;
			oHeight = _height;
		}

		virtual bool Initialize(int32_t Width, int32_t Height, void* hInstance=nullptr, AppFlags Flags = AppFlags::None) = 0;
		virtual void* GetOSWindow() = 0;
		virtual void DrawImageToWindow(int32_t Width, int32_t Height, const void* InData, int32_t InDataSize, uint8_t BPP) = 0;
		virtual int32_t Run() = 0;
		virtual int32_t RunOnce() = 0;

		void SetEvents(const ApplicationEvents& InEvents)
		{
			*(ApplicationEvents*)(this) = InEvents;
		}

		void SetInputEvents(const InputEvents& InEvents)
		{
			*(InputEvents*)(this) = InEvents;
		}
	};

	SPP_ENGINE_API std::unique_ptr< ApplicationWindow > CreateApplication(const char* appType=nullptr);
}