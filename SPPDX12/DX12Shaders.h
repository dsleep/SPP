// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "DX12Device.h"

namespace SPP
{
	class D3D12Shader : public GPUShader
	{
	protected:
		ComPtr<IDxcBlob>				_shader;
		ComPtr<ID3D12RootSignature>		_rootSignature;
		D3D12_SHADER_DESC				_desc;

	public:
		D3D12Shader(EShaderType InType);

		virtual int32_t GetInstructionCount() const override;

		ID3D12RootSignature* GetRootSignature() const;
		static const char* ReturnDXType(EShaderType inType);
		D3D12_SHADER_BYTECODE GetByteCode();
		virtual void UploadToGpu() override;
		int32_t CreateAndWaitForProcess(const char* ProcessPath, const char* Commandline);
		virtual bool CompileShaderFromString(const std::string& ShaderSource, const char* ShaderName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr) override;
			
	};

}