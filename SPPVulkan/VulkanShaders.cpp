// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "VulkanShaders.h"
#include "VulkanTools.h"

#include "SPPLogging.h"
#include "SPPFileSystem.h"

#include "SPPPlatformCore.h"
#include <thread>

namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;

	LogEntry LOG_VULKANSHADER("VulkanShader");

	const char* ReturnTargetString(EShaderType inType)
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

	VulkanShader::VulkanShader(EShaderType InType) : GPUShader(InType)
	{
		SE_ASSERT(InType == EShaderType::Pixel || InType == EShaderType::Vertex);		
	}

	bool VulkanShader::CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint, std::string* oErrorMsgs)
	{
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "CompileShaderFromFile: %s(%s)", *FileName, EntryPoint);

		AssetPath shaderRoot("shaders");
		AssetPath shaderBinObject( (std::string("CACHE\\") + FileName.GetName() + ".SPIRV").c_str());
		AssetPath shaderBuildOutput((std::string("CACHE\\") + FileName.GetName() + ".txt").c_str());

		std::string FullDXCPath = SPP::GRootPath + "3rdParty/dxc/bin/dxc.exe";

		std::string CommandString = std::string_format("\"%s\" -spirv -fspv-debug=line -fspv-reflect -Zi -T %s -E %s -I \"%s\" -Fo \"%s\"",
			*FileName,
			ReturnTargetString(_type),
			EntryPoint,
			*shaderRoot,
			*shaderBinObject
			);

		{
			auto ShaderProcess = CreatePlatformProcess(FullDXCPath.c_str(), CommandString.c_str(), false, true);

			using namespace std::chrono_literals;
			if (ShaderProcess->IsValid())
			{
				auto ProcessOutput = ShaderProcess->GetOutput();

				if (!ProcessOutput.empty())
				{
					SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%s", ProcessOutput.c_str());
					return false;
				}
			}
		}

		std::vector<uint8_t> FileData;
		if (LoadFileToArray(*shaderBinObject, FileData))
		{
			VkShaderModuleCreateInfo moduleCreateInfo{};
			moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			moduleCreateInfo.codeSize = FileData.size();
			moduleCreateInfo.pCode = (uint32_t*)FileData.data();

			VkResult results = vkCreateShaderModule(GGlobalVulkanDevice, &moduleCreateInfo, NULL, &_shader);
			if (results != VK_SUCCESS)
			{
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "CompileShaderFromFile: failed vkCreateShaderModule");
				return false;
			}
		}

		return true;
	}
	
	bool VulkanShader::CompileShaderFromString(const std::string& ShaderSource, const char* ShaderName, const char* EntryPoint, std::string* oErrorMsgs)
	{
		return false;
	}

	GPUReferencer< GPUShader > Vulkan_CreateShader(EShaderType InType)
	{
		return Make_GPU< VulkanShader >(InType);
	}
}