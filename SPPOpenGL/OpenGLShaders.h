// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "OpenGLDevice.h"

namespace SPP
{
	class OpenGLShader : public GPUShader
	{
	protected:

	public:
		OpenGLShader(EShaderType InType);


		virtual void UploadToGpu() override { }
		virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main") override { return false; }
	};

}