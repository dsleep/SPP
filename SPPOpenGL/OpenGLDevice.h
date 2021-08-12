// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <GL/glew.h>

#include "SPPCore.h"
#include "SPPString.h"
#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"
#include "SPPCamera.h"

#include "OpenGLUtils.h"

namespace SPP
{
	class OpenGLDevice : public GraphicsDevice
	{
	protected:

	public:

		virtual void Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow) override { }
		virtual void ResizeBuffers(int32_t NewWidth, int32_t NewHeight) override { }

		virtual int32_t GetDeviceWidth() const override { return -1; }
		virtual int32_t GetDeviceHeight() const override { return -1; }

		virtual void BeginFrame() override { }
		virtual void EndFrame() override { }
		virtual void MoveToNextFrame() { };
		
	};
}