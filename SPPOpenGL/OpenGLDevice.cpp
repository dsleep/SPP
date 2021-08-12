// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "OpenGLDevice.h"

#include "OpenGLShaders.h"
#include "OpenGLBuffers.h"
#include "OpenGLTextures.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"


namespace SPP
{
	LogEntry LOG_OPENGL("OpenGL");

	// lazy externs
	extern std::shared_ptr< GPUShader > OpenGL_CreateShader(EShaderType InType);
	//extern std::shared_ptr< GPUComputeDispatch> OpenGL_CreateComputeDispatch(std::shared_ptr< GPUShader> InCS);
	extern std::shared_ptr< GPUBuffer > OpenGL_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	//extern std::shared_ptr< GPUInputLayout > OpenGL_CreateInputLayout();
	//extern std::shared_ptr< GPURenderTarget > OpenGL_CreateRenderTarget();
	extern std::shared_ptr< GPUTexture > OpenGL_CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);
	//
	std::shared_ptr< GraphicsDevice > OpenGL_CreateGraphicsDevice()
	{
		return std::make_shared< OpenGLDevice>();
	}
		
	struct OpenGLGraphicInterface : public IGraphicsInterface
	{
		// hacky so one GGI per DLL
		OpenGLGraphicInterface()
		{
			SET_GGI(this);
		}

		virtual std::shared_ptr< GPUShader > CreateShader(EShaderType InType) override
		{			
			return OpenGL_CreateShader(InType);
		}
		virtual std::shared_ptr< GPUComputeDispatch > CreateComputeDispatch(std::shared_ptr< GPUShader> InCS) override
		{
			return nullptr;
		}
		virtual std::shared_ptr< GPUBuffer > CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData = nullptr) override
		{
			return OpenGL_CreateStaticBuffer(InType, InCpuData);
		}
		virtual std::shared_ptr< GPUInputLayout > CreateInputLayout() override
		{
			return nullptr;
		}
		virtual std::shared_ptr< GPUTexture > CreateTexture(int32_t Width, int32_t Height, TextureFormat Format,
			std::shared_ptr< ArrayResource > RawData = nullptr,
			std::shared_ptr< ImageMeta > InMetaInfo = nullptr) override
		{
			return OpenGL_CreateTexture(Width, Height, Format, RawData, InMetaInfo);
		}
		virtual std::shared_ptr< GPURenderTarget > CreateRenderTarget() override
		{
			return nullptr;
		}
		virtual std::shared_ptr< GraphicsDevice > CreateGraphicsDevice() override
		{
			return OpenGL_CreateGraphicsDevice();
		}
		virtual std::shared_ptr<RenderScene> CreateRenderScene() override
		{
			return nullptr;
		}
	};

	static OpenGLGraphicInterface staticDGI;
}