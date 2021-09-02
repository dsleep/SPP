// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "DX12Shaders.h"
#include "SPPLogging.h"

#include <exception>

#pragma comment( lib, "dxcompiler.lib" )

namespace SPP
{
	LogEntry LOG_D3D12Shader("D3D12Shader");
	
	D3D12Shader::D3D12Shader(EShaderType InType) : GPUShader(InType)
	{
	}

	ID3D12RootSignature* D3D12Shader::GetRootSignature() const
	{
		return _rootSignature.Get();
	}

	const char* D3D12Shader::ReturnDXType(EShaderType inType)
	{
		switch (inType)
		{
		case EShaderType::Pixel:
			return "ps_6_5";
		case EShaderType::Vertex:
			return "vs_6_5";
		case EShaderType::Compute:
			return "cs_6_5";
		case EShaderType::Domain:
			return "ds_6_5";
		case EShaderType::Hull:
			return "hs_6_5";
		case EShaderType::Mesh:
			return "ms_6_5";
		case EShaderType::Amplification:
			return "as_6_5";
		}
		return "none";
	}

	D3D12_SHADER_BYTECODE D3D12Shader::GetByteCode()
	{
		SE_ASSERT(_shader);
		return D3D12_SHADER_BYTECODE({ _shader->GetBufferPointer(), _shader->GetBufferSize() });
	}

	void D3D12Shader::UploadToGpu()
	{

	}

	int32_t D3D12Shader::CreateAndWaitForProcess(const char* ProcessPath, const char* Commandline)
	{
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		//si.dwFlags |= STARTF_USESTDHANDLES;
		//si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		//si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);

		if (CreateProcessA(ProcessPath, (LPSTR)Commandline,
			NULL, NULL, TRUE,
			0, NULL, NULL, &si, &pi) == false)
		{
			printf("CreateProcess failed: error %d\n", GetLastError());
			return -1;
		}

		// Wait until child process exits.
		DWORD dwExitCode = WaitForSingleObject(pi.hProcess, INFINITE);

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		return dwExitCode;
	}

	std::string D3D12Shader::read_to_string(const char* filename)
	{
		std::ifstream fs(filename);
		std::stringstream ss;

		std::string line;
		while (std::getline(fs, line))
		{
			ss << line << "\n";
		}
		return ss.str();
	}

	bool D3D12Shader::CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint)
	{
		SPP_LOG(LOG_D3D12Shader, LOG_INFO, "CompileShaderFromFile: %s(%s) type %s", *FileName, EntryPoint, ReturnDXType(_type));

		AssetPath shaderRoot("shaders");

		ComPtr<IDxcUtils> pLibrary;
		DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(pLibrary.GetAddressOf()));
		ComPtr<IDxcCompiler3> pCompiler;
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(pCompiler.GetAddressOf()));
		ComPtr<IDxcIncludeHandler> pincludeHandler;
		pLibrary->CreateDefaultIncludeHandler(&pincludeHandler);

		std::string sourceString = read_to_string(*FileName);
		std::wstring EntryPointW = std::utf8_to_wstring(EntryPoint);
		std::wstring IncludePathW = std::utf8_to_wstring(*shaderRoot);

		std::vector<const wchar_t*> arguments;

		//-E for the entry point (eg. PSMain)
		arguments.push_back(L"-E");
		arguments.push_back(EntryPointW.c_str());

		arguments.push_back(L"-I");
		arguments.push_back(IncludePathW.c_str());

		//-T for the target profile (eg. ps_6_2)
		std::wstring TargetW = std::utf8_to_wstring(ReturnDXType(_type));
		arguments.push_back(L"-T");
		arguments.push_back(TargetW.c_str());

		arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS); //-WX
		arguments.push_back(DXC_ARG_DEBUG); //-Zi
		arguments.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR); //-Zp
		arguments.push_back(DXC_ARG_SKIP_OPTIMIZATIONS); //-Zp

		arguments.push_back(L"-Qembed_debug");

		//for (const std::wstring& define : defines)
		//{
		//	arguments.push_back(L"-D");
		//	arguments.push_back(define.c_str());
		//}

		DxcBuffer sourceBuffer;
		sourceBuffer.Ptr = sourceString.c_str();
		sourceBuffer.Size = sourceString.length();
		sourceBuffer.Encoding = 0;

		std::vector<DxcDefine> dxcDefines;

		ComPtr<IDxcResult> pCompileResult;

		try
		{			
			pCompiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), pincludeHandler.Get(), IID_PPV_ARGS(pCompileResult.GetAddressOf()));
		}
		catch (std::exception& e)
		{
			SPP_LOG(LOG_D3D12Shader, LOG_INFO, " - execption %s", e.what());

		}
		

		HRESULT hrCompilation = 0;
		pCompileResult->GetStatus(&hrCompilation);

		if (hrCompilation < 0)
		{
			//Error Handling
			ComPtr<IDxcBlobUtf8> pErrors;
			pCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(pErrors.GetAddressOf()), nullptr);
			if (pErrors && pErrors->GetStringLength() > 0)
			{
				OutputDebugStringA((char*)pErrors->GetBufferPointer());
			}
			SPP_LOG(LOG_D3D12Shader, LOG_INFO, " - FAILED");
			return false;
		}
				
		pCompileResult->GetResult(&_shader);

		SPP_LOG(LOG_D3D12Shader, LOG_INFO, " - SUCCESS SIZE %d", _shader->GetBufferSize());

		auto pd3dDevice = GGraphicsDevice->GetDevice();

		// Pull root signature from the precompiled mesh shader.
		if (_type == EShaderType::Vertex || _type == EShaderType::Mesh || _type == EShaderType::Compute)
		{
			ThrowIfFailed(pd3dDevice->CreateRootSignature(0, _shader->GetBufferPointer(), _shader->GetBufferSize(), IID_PPV_ARGS(&_rootSignature)));
		}

		return true;
	}

	std::shared_ptr< GPUShader > DX12_CreateShader(EShaderType InType)
	{
		return std::make_shared < D3D12Shader >(InType);
	}
}