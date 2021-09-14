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

		GLenum err = glewInit();
		if (GLEW_OK != err)
		{
			/* Problem: glewInit failed, something is seriously wrong. */
			SPP_LOG(LOG_OPENGL, LOG_INFO, "Error: %s", glewGetErrorString(err));
		}
		SPP_LOG(LOG_OPENGL, LOG_INFO, "Status: Using GLEW %s", glewGetString(GLEW_VERSION));
	}
	void OpenGLDevice::ResizeBuffers(int32_t NewWidth, int32_t NewHeight)
	{
		_deviceWidth = NewWidth;
		_deviceHeight = NewHeight;
	}

	void OpenGLDevice::BeginFrame() {}
	void OpenGLDevice::EndFrame() {}

	class OpenGLProgramState
	{
	protected:
		GLuint _programID = 0;

	public:
		OpenGLProgramState()
		{
			_programID = glCreateProgram();
		}

		~OpenGLProgramState()
		{
			glDeleteProgram(_programID);
			_programID = 0;
		}

		GLuint GetProgramID() const 
		{
			return _programID;
		}

		void Initialize(
			std::shared_ptr< GPUShader> InVS,
			std::shared_ptr< GPUShader> InPS)
		{
			// Link the program
			SPP_LOG(LOG_OPENGL, LOG_INFO, "Linking program");
			glAttachShader(_programID, InVS->GetAs<OpenGLShader>().GetShaderID());
			glAttachShader(_programID, InPS->GetAs<OpenGLShader>().GetShaderID());
			glLinkProgram(_programID);

			GLint Result = GL_FALSE;
			int InfoLogLength;

			// Check the program
			glGetProgramiv(_programID, GL_LINK_STATUS, &Result);
			glGetProgramiv(_programID, GL_INFO_LOG_LENGTH, &InfoLogLength);
			if (InfoLogLength > 0)
			{
				std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
				glGetProgramInfoLog(_programID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
				SPP_LOG(LOG_OPENGL, LOG_INFO, "%s", &ProgramErrorMessage[0]);
			}

			GLint i;
			GLint count;

			GLint size; // size of the variable
			GLenum type; // type of the variable (float, vec3 or mat4, etc)

			const GLsizei bufSize = 64; // maximum name length
			GLchar name[bufSize]; // variable name in GLSL
			GLsizei length; // name length

			glGetProgramiv(_programID, GL_ACTIVE_ATTRIBUTES, &count);
			SPP_LOG(LOG_OPENGL, LOG_INFO, "Active Attributes: %d", count);

			glUseProgram(_programID);

			for (i = 0; i < count; i++)
			{
				glGetActiveAttrib(_programID, (GLuint)i, bufSize, &length, &size, &type, name);
				SPP_LOG(LOG_OPENGL, LOG_INFO, "Attribute #%d Type: 0x%X Name: %s : %d LOC: %d", i, type, name, size, glGetAttribLocation(_programID, name));
				//AttributeMap[name] = GLAttribute(name, glGetAttribLocation(ProgramID, name), type, size);
			}

			glGetProgramiv(_programID, GL_ACTIVE_UNIFORMS, &count);
			SPP_LOG(LOG_OPENGL, LOG_INFO, "Active Uniforms : %d", count);

			for (i = 0; i < count; i++)
			{
				glGetActiveUniform(_programID, (GLuint)i, bufSize, &length, &size, &type, name);
				SPP_LOG(LOG_OPENGL, LOG_INFO, "Uniform #%d Type: 0x%X Name: %s : %d : LOC: %d", i, type, name, size, glGetUniformLocation(_programID, name));
			}
		}
	};

	struct OpenGLProgramKey
	{
		uintptr_t vs = 0;
		uintptr_t ps = 0;

		bool operator<(const OpenGLProgramKey& compareKey)const
		{
			if (vs != compareKey.vs)
			{
				return vs < compareKey.vs;
			}
			if (ps != compareKey.ps)
			{
				return ps < compareKey.ps;
			}

			return false;
		}
	};

	static std::map< OpenGLProgramKey, std::shared_ptr< OpenGLProgramState > > PiplineStateMap;

	std::shared_ptr < OpenGLProgramState >  GetOpenGLProgramState(
		std::shared_ptr< GPUShader> InVS,
		std::shared_ptr< GPUShader> InPS)
	{
		OpenGLProgramKey key{ 	
			(uintptr_t)InVS.get(),
			(uintptr_t)InPS.get() };

		auto findKey = PiplineStateMap.find(key);

		if (findKey == PiplineStateMap.end())
		{
			auto newPipelineState = std::make_shared< OpenGLProgramState >();
			newPipelineState->Initialize(InVS, InPS);
			PiplineStateMap[key] = newPipelineState;
			return newPipelineState;
		}

		return findKey->second;
	}

	static const GLfloat g_vertex_buffer_data[] = {
   -1.0f, -1.0f, 0.0f,
   1.0f, -1.0f, 0.0f,
   0.0f,  1.0f, 0.0f,
	};

	class OpenGLScene : public RenderScene
	{
	protected:
		std::shared_ptr< GPUShader > pixelShader;
		std::shared_ptr< GPUShader > vertexShader;
		GLuint vertexbuffer;

	public:
		OpenGLScene()
		{
			pixelShader = GGI()->CreateShader(EShaderType::Pixel);
			pixelShader->CompileShaderFromFile("shaders/OpenGL/FullScene.hlsl", "main");

			vertexShader = GGI()->CreateShader(EShaderType::Vertex);
			vertexShader->CompileShaderFromFile("shaders/OpenGL/FullScreenQuad.vs", "main");

			// This will identify our vertex buffer
			
			// Generate 1 buffer, put the resulting identifier in vertexbuffer
			glGenBuffers(1, &vertexbuffer);
			// The following commands will talk about our 'vertexbuffer' buffer
			glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
			// Give our vertices to OpenGL.
			glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
		}
		virtual ~OpenGLScene() { }
		virtual void BeginFrame()
		{

		}
		virtual void Draw()
		{
			auto ViewportX = GGraphicsDevice->GetDeviceWidth();
			auto ViewportY = GGraphicsDevice->GetDeviceHeight();
			glViewport(0, 0, ViewportX, ViewportY);
			glClearColor(0, 0, 1.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			auto stateKey = GetOpenGLProgramState(vertexShader, pixelShader);

			// be sure to activate the shader
			glUseProgram(stateKey->GetProgramID());

			static float timeAdd = 0;

			glUniform1i(0, 0);
			glUniform2f(1, ViewportX/2, ViewportY/2);
			glUniform2f(2, ViewportX, ViewportY);
			glUniform1f(3, timeAdd);

			timeAdd += 0.0016f;

			glDrawArrays(GL_TRIANGLES, 0, 6); // Starting from vertex 0; 3 vertices total -> 1 triangle
			
			glUseProgram(0);
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
		virtual std::shared_ptr<RenderableMesh> CreateRenderableMesh() override
		{
			return nullptr;
		}
		virtual std::shared_ptr< class RenderableSignedDistanceField > CreateRenderableSDF() override
		{
			return nullptr;
		}
	};

	static OpenGLGraphicInterface staticDGI;
}