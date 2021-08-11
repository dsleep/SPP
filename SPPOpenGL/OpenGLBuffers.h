// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "OpenGLDevice.h"

namespace SPP
{	
	class OpenGLBuffer : public GPUBuffer
	{
	protected:

	public:
		OpenGLBuffer(std::shared_ptr< ArrayResource > InCpuData);

		virtual ~OpenGLBuffer();
	
	};
	
}