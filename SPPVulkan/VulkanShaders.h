// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "VulkanDevice.h"

namespace SPP
{
	struct DescriptorSetLayoutData 
	{
		uint32_t set_number;
		std::vector<VkDescriptorSetLayoutBinding> bindings;
	};

	class VulkanShader : public GPUShader
	{
	private:
		VkShaderModule _shader = nullptr;
		std::vector<DescriptorSetLayoutData> _layoutSets;

	public:
		VulkanShader(EShaderType InType);

		VkShaderModule GetModule() const {
			return _shader;
		}

		const std::vector<DescriptorSetLayoutData>& GetLayoutSets() const {
			return _layoutSets;
		}

		virtual void UploadToGpu() override { };
		virtual bool CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr) override;
		virtual bool CompileShaderFromString(const std::string& ShaderSource, const char* ShaderName, const char* EntryPoint = "main", std::string* oErrorMsgs = nullptr) override;
	};
}
