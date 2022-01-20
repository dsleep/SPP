// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "VulkanDevice.h"

namespace SPP
{
	class VulkanShader : public GPUShader
	{
	private:
		VkShaderModule _shader = nullptr;

	public:
		VulkanShader(EShaderType InType);

		virtual void UploadToGpu() override { };
		virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr) override;
		virtual bool CompileShaderFromString(const std::string& ShaderSource, const char* ShaderName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr) override;
	};
}
