// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "DX12Shaders.h"
#include "SPPLogging.h"
#include "SPPFileSystem.h"

#include <exception>

#pragma comment( lib, "dxcompiler.lib" )

namespace SPP
{
	LogEntry LOG_D3D12Shader("D3D12Shader");
	
	//class IncPathIncludeHandler : public IDxcIncludeHandler
	//{
	//public:
	//	virtual ~IncPathIncludeHandler() {}

	//	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	//	{
	//		SE_ASSERT(false);
	//		// used?!
	//		return E_FAIL;// DoBasicQueryInterface<::IDxcIncludeHandler>(this, riid, ppvObject);
	//	}

	//	IncPathIncludeHandler(IDxcUtils* library) : m_dwRef(0), _library(library)
	//	{}

	//	HRESULT STDMETHODCALLTYPE LoadSource( LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override
	//	{
	//		std::string sourceName = std::wstring_to_utf8(pFilename);

	//		std::string FileData;
	//		if (LoadFileToString(sourceName.c_str(), FileData))
	//		{
	//			ComPtr<IDxcBlobEncoding> pEncodingIncludeSource;
	//			_library->CreateBlob(FileData.data(), FileData.size(), CP_UTF8, &pEncodingIncludeSource);
	//			*ppIncludeSource = pEncodingIncludeSource.Detach();
	//			return S_OK;
	//		}

	//		return E_FAIL;
	//	}

	//	ULONG STDMETHODCALLTYPE AddRef()
	//	{
	//		return InterlockedIncrement(&m_dwRef);
	//	}

	//	ULONG STDMETHODCALLTYPE Release()
	//	{
	//		ULONG result = InterlockedDecrement(&m_dwRef);
	//		if (result == 0) delete this;
	//		return result;
	//	}

	//private:
	//	volatile ULONG m_dwRef = 0;
	//	IDxcUtils* _library = nullptr;
	//};

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

	bool D3D12Shader::CompileShaderFromString(const std::string& ShaderSource, const char *ShaderName, const char* EntryPoint, std::string* oErrorMsgs)
	{
		SPP_LOG(LOG_D3D12Shader, LOG_INFO, "CompileShaderFromFile: %s(%s) type %s", ShaderName, EntryPoint, ReturnDXType(_type));

		AssetPath shaderRoot("shaders");

		ComPtr<IDxcUtils> pLibrary;
		DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(pLibrary.GetAddressOf()));
		ComPtr<IDxcCompiler3> pCompiler;
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(pCompiler.GetAddressOf()));
		ComPtr<IDxcIncludeHandler> pincludeHandler;
		pLibrary->CreateDefaultIncludeHandler(&pincludeHandler);
		
		//std::string sourceString = read_to_string(*FileName);
		std::wstring sourceName = std::utf8_to_wstring(ShaderName);
		std::wstring EntryPointW = std::utf8_to_wstring(EntryPoint);
		std::wstring IncludePathW = std::utf8_to_wstring(*shaderRoot);
		
		std::vector<const wchar_t*> arguments;

		arguments.push_back(sourceName.c_str());

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
		sourceBuffer.Ptr = ShaderSource.c_str();
		sourceBuffer.Size = ShaderSource.length();
		sourceBuffer.Encoding = 0;

		std::vector<DxcDefine> dxcDefines;

		ComPtr<IDxcResult> pCompileResult;

		//IncPathIncludeHandler customINclude(pLibrary.Get());
		//customINclude.AddRef();

		try
		{			
			pCompiler->Compile(&sourceBuffer, 
				arguments.data(),
				(uint32_t)arguments.size(), 
				pincludeHandler.Get(), 
				IID_PPV_ARGS(pCompileResult.GetAddressOf()));
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
				if (oErrorMsgs)
				{
					*oErrorMsgs = (char*)pErrors->GetBufferPointer();
				}
				OutputDebugStringA((char*)pErrors->GetBufferPointer());
			}
			SPP_LOG(LOG_D3D12Shader, LOG_INFO, " - FAILED");
			return false;
		}
				
		pCompileResult->GetResult(&_shader);


		auto bufPtr = _shader->GetBufferPointer();
		auto bufSize = _shader->GetBufferSize();


		ComPtr<ID3DBlob> pPDBName;
		D3DGetBlobPart(bufPtr, bufSize, D3D_BLOB_DEBUG_NAME, 0, pPDBName.GetAddressOf());

		auto pDebugNameData = reinterpret_cast<void*>(pPDBName->GetBufferPointer());

		SPP_LOG(LOG_D3D12Shader, LOG_INFO, " - SUCCESS SIZE %d", _shader->GetBufferSize());

		auto pd3dDevice = GGraphicsDevice->GetDevice();

		// Pull root signature from the precompiled mesh shader.
		if (_type == EShaderType::Vertex || _type == EShaderType::Mesh || _type == EShaderType::Compute)
		{
			ThrowIfFailed(pd3dDevice->CreateRootSignature(0, _shader->GetBufferPointer(), _shader->GetBufferSize(), IID_PPV_ARGS(&_rootSignature)));
		}

		return true;
	}

	GPUReferencer< GPUShader > DX12_CreateShader(EShaderType InType)
	{
		return GPUReferencer< GPUShader >( new D3D12Shader(InType) );
	}
}