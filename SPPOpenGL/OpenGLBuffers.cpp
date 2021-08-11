// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "OpenGLBuffers.h"
#include "SPPMesh.h"

namespace SPP
{	
	OpenGLBuffer::OpenGLBuffer(std::shared_ptr< ArrayResource > InCpuData) : GPUBuffer(InCpuData)
	{
		
	}

	OpenGLBuffer::~OpenGLBuffer()
	{

	}
}