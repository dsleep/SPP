// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "OpenGLShaders.h"
#include "SPPLogging.h"

namespace SPP
{
	LogEntry LOG_OpenGLShader("OpenGLShader");
	
	OpenGLShader::OpenGLShader(EShaderType InType) : GPUShader(InType)
	{
		SE_ASSERT(InType == EShaderType::Pixel || InType == EShaderType::Vertex);
		_shaderID = glCreateShader(InType == EShaderType::Pixel ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
	}


	OpenGLShader::~OpenGLShader()
	{
		glDeleteShader(_shaderID);
	}

	void OpenGLShader::UploadToGpu()
	{

	}

	bool OpenGLShader::CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint)
	{
		std::string ShaderCode;
		std::ifstream ShaderStream(*FileName, std::ios::in);
		if (ShaderStream.is_open())
		{
			std::stringstream sstr;
			sstr << ShaderStream.rdbuf();
			ShaderCode = sstr.str();
			ShaderStream.close();
		}
		else
		{
			printf("Impossible to open %s. Are you in the right directory ? Don't forget to read the FAQ !\n", *FileName);
			return false;
		}

		GLint Result = GL_FALSE;
		int InfoLogLength;

		// Compile Vertex Shader
		SPP_LOG(LOG_OpenGLShader, LOG_INFO, "Compiling shader : %s", *FileName);
		char const* SourcePointer = ShaderCode.c_str();
		glShaderSource(_shaderID, 1, &SourcePointer, NULL);
		glCompileShader(_shaderID);

		// Check Vertex Shader
		glGetShaderiv(_shaderID, GL_COMPILE_STATUS, &Result);
		glGetShaderiv(_shaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
		if (InfoLogLength > 0)
		{
			std::vector<char> ShaderErrorMessage(InfoLogLength + 1);
			glGetShaderInfoLog(_shaderID, InfoLogLength, NULL, &ShaderErrorMessage[0]);
			SPP_LOG(LOG_OpenGLShader, LOG_INFO, "%s", &ShaderErrorMessage[0]);
		}

		return true;
	}

	std::shared_ptr< GPUShader > OpenGL_CreateShader(EShaderType InType)
	{
		return std::make_shared < OpenGLShader >(InType);
	}
}