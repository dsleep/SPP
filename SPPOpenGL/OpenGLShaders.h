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
		GLuint _shaderID = 0;

	public:
		OpenGLShader(EShaderType InType);
		virtual ~OpenGLShader();

		virtual void UploadToGpu() override;
		virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main") override;
	};

	template<GLint ShaderType>
	class TGLShader
	{
	protected:
		GLuint ShaderID = 0;

	public:
		TGLShader()
		{
			ShaderID = glCreateShader(ShaderType);
		}

		void CompileFromFile(const char* FilePath);

		~TGLShader()
		{
			glDeleteShader(ShaderID);
			ShaderID = 0;
		}

		GLuint GetShaderID()
		{
			return ShaderID;
		}
	};

	struct GLTextureSampler2D
	{
		std::weak_ptr<class GLTexture2D> GLTexture;

		GLTextureSampler2D& operator=(std::shared_ptr<class GLTexture2D> InTexture)
		{
			GLTexture = InTexture;
			return *this;
		}
	};

	class GLUniformParameters
	{
	protected:
		GLint UniformIndex;
		std::string ParamName;
		uint32_t DrawUpdate;
		int32_t TypeID = -1;

	public:
		GLUniformParameters(GLint InGLIndex, const char* InParamName) : UniformIndex(InGLIndex), ParamName(InParamName) {}

		virtual ~GLUniformParameters() { }

		GLint GetUniformIndex()
		{
			return UniformIndex;
		}

		void SetTypeID(int32_t InTypeID)
		{
			TypeID = InTypeID;
		}

		void SetUniformIndex(GLint InUniformIndex)
		{
			UniformIndex = InUniformIndex;
		}

		template<typename RawType, typename ActualType>
		void TSet(const ActualType& InValue);

		virtual void Apply() = 0;

		//TODO do anything?
		virtual void Clear() { }
	};


	template<typename StorageType>
	struct TGLUniformParameters : GLUniformParameters
	{
	protected:
		StorageType StoredValue;

	public:
		TGLUniformParameters(GLint InGLIndex, const char* InParamName);

		virtual ~TGLUniformParameters() { }

		template<typename ActualType>
		void Set(const ActualType& InValue)
		{
			StoredValue = InValue;
		}

		virtual void Apply() override;
	};

	template<typename RawType, typename ActualType>
	void GLUniformParameters::TSet(const ActualType& InValue)
	{
		if (TypeID == GetGLM_as_ID<RawType>())
		{
			static_cast<TGLUniformParameters< typename TConvertUniformRawToStorage<RawType>::StorageType >*>(this)->Set(InValue);
		}
		else
		{
			assert(0);
		}
	}

	class GLAttribute
	{
	private:
		std::string ParamName;
		GLuint AttributeLocation = 0;
		GLenum AttributeType = 0;
		GLint AttributeStride = -1;
		uint8_t AttributeOffset = 0;

		std::weak_ptr<class GLBuffer> Buffer;

	public:
		GLAttribute(const char* Name,
			GLuint InAttributeLocation,
			GLenum InAttributeType,
			GLint InAttributeStride) : ParamName(Name), AttributeLocation(InAttributeLocation), AttributeType(InAttributeType), AttributeStride(InAttributeStride) {}

		GLAttribute() {}

		void SetBuffer(std::weak_ptr<class GLBuffer> InBuffer, int32_t InAttributeStride, uint8_t InAttributeOffset)
		{
			Buffer = InBuffer;
			AttributeOffset = InAttributeOffset;
			AttributeStride = InAttributeStride;
		}

		void Apply();
		void Clear();
	};

	//struct GLShaderProgram
	//{
	//	GLuint ProgramID = 0;
	//	std::shared_ptr< TGLShader< GL_FRAGMENT_SHADER > > PixelShader;
	//	std::shared_ptr< TGLShader< GL_VERTEX_SHADER > > VertexShader;

	//	std::map< std::string, GLAttribute > AttributeMap;
	//	std::map< std::string, std::shared_ptr<GLUniformParameters> > UniformMap;

	//	GLShaderProgram()
	//	{
	//		ProgramID = glCreateProgram();
	//	}

	//	~GLShaderProgram()
	//	{
	//		glDeleteProgram(ProgramID);
	//		ProgramID = 0;
	//	}

	//	void SetAttribute(const char* UniformName, std::shared_ptr<class GLBuffer> Buffer, int32_t InAttributeStride = 0, uint8_t InAttributeOffset = 0)
	//	{
	//		auto Attribute = AttributeMap.find(UniformName);
	//		if (Attribute != AttributeMap.end())
	//		{
	//			Attribute->second.SetBuffer(Buffer, InAttributeStride, InAttributeOffset);
	//		}
	//	}

	//	GLuint GetUniformLocation(const char* UniformName)
	//	{
	//		auto Uniform = UniformMap.find(UniformName);
	//		if (Uniform != UniformMap.end())
	//		{
	//			return Uniform->second->GetUniformIndex();
	//		}
	//		return 0;
	//	}


	//	// support for direct and pointer grab	
	//	template <typename RawValue>
	//	void SetUniform(const char* UniformName, const RawValue& InValue)
	//	{
	//		auto Uniform = UniformMap.find(UniformName);
	//		if (Uniform != UniformMap.end())
	//		{
	//			Uniform->second->TSet< remove_pointer<RawValue>, RawValue >(InValue);
	//		}
	//	}

	//	void Apply();
	//	void Clear();

	//	void LinkAndLoad(const std::string& VS_FilePath, const std::string& FS_FilePath);
	//};
}