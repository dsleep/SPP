// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#ifdef _WIN32
#include <windows.h>	
#endif

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


	class GraphicsDevice* GGraphicsDevice = nullptr;

	OpenGLDevice::OpenGLDevice()
	{
		SE_ASSERT(GGraphicsDevice == nullptr );
		GGraphicsDevice = this;
	}

	OpenGLDevice::~OpenGLDevice()
	{
		GGraphicsDevice = nullptr;
	}

	void OpenGLDevice::Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow)
	{
		_deviceWidth = InitialWidth;
		_deviceHeight = InitialHeight;
		_hwndPtr = OSWindow;


#ifdef _WIN32
		auto hDC = GetDC((HWND)OSWindow);
		auto hRC = wglCreateContext(hDC);
		wglMakeCurrent(hDC, hRC);
#endif
	}
	void OpenGLDevice::ResizeBuffers(int32_t NewWidth, int32_t NewHeight)
	{
		_deviceWidth = NewWidth;
		_deviceHeight = NewHeight;
	}

	void OpenGLDevice::BeginFrame() {}
	void OpenGLDevice::EndFrame() {}


	class OpenGLScene : public RenderScene
	{
	protected:

	private:
		virtual void BeginFrame()
		{

		}
		virtual void Draw()
		{
			glViewport(0, 0, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
			glClear(GL_COLOR_BUFFER_BIT);


			glFlush();
		}
		virtual void EndFrame()
		{

		}
	};

	std::shared_ptr< RenderScene > OpenGL_CreateRenderScene()
	{
		return std::make_shared< OpenGLScene>();
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
			return OpenGL_CreateRenderScene();
		}
	};

	static OpenGLGraphicInterface staticDGI;
}