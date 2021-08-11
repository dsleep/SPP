// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "OpenGLShaders.h"
#include "SPPLogging.h"

#define REL_SHADER_PATH "shaders"

namespace SPP
{
	LogEntry LOG_OpenGLShader("OpenGLShader");
	
	OpenGLShader::OpenGLShader(EShaderType InType) : GPUShader(InType)
	{
	}


	std::shared_ptr< GPUShader > OpenGL_CreateShader(EShaderType InType)
	{
		return std::make_shared < OpenGLShader >(InType);
	}
}