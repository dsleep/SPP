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

	public:
		D3D12Shader(EShaderType InType);

		ID3D12RootSignature* GetRootSignature() const;
		static const char* ReturnDXType(EShaderType inType);
		D3D12_SHADER_BYTECODE GetByteCode();
		virtual void UploadToGpu() override;
		int32_t CreateAndWaitForProcess(const char* ProcessPath, const char* Commandline);
		std::string read_to_string(const char* filename);
		virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main") override;
	};

}