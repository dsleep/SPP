// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "OpenGLBuffers.h"
#include "SPPMesh.h"

namespace SPP
{	
	OpenGLBuffer::OpenGLBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData) : GPUBuffer(InCpuData)
	{
		glGenBuffers(1, &_bufferID);
	}

	OpenGLBuffer::~OpenGLBuffer()
	{
		glDeleteBuffers(1, &_bufferID);
	}

	std::shared_ptr< GPUBuffer > OpenGL_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData)
	{
		return std::make_shared< OpenGLBuffer>(InType, InCpuData);
	}
}