// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "VulkanShaders.h"
#include "VulkanTools.h"

#include "SPPLogging.h"
#include "SPPFileSystem.h"

#include "spirvreflect/spirv_reflect.h"
#include "spirvreflect/spirv_output_stream.h"

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

	void PrintModuleInfo(std::ostream& os, const SpvReflectShaderModule& obj, const char* /*indent*/)
	{
		os << "entry point     : " << obj.entry_point_name << "\n";
		os << "source lang     : " << spvReflectSourceLanguage(obj.source_language) << "\n";
		os << "source lang ver : " << obj.source_language_version << "\n";
		if (obj.source_language == SpvSourceLanguageHLSL) {
			os << "stage           : ";
			switch (obj.shader_stage) {
			default: break;
			case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT: os << "VS"; break;
			case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT: os << "HS"; break;
			case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: os << "DS"; break;
			case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT: os << "GS"; break;
			case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT: os << "PS"; break;
			case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT: os << "CS"; break;
			}
		}
	}

	void PrintDescriptorBinding(std::ostream& os, const SpvReflectDescriptorBinding& obj, bool write_set, const char* indent)
	{
		const char* t = indent;
		os << t << "binding : " << obj.binding << "\n";
		if (write_set) {
			os << t << "set     : " << obj.set << "\n";
		}
		os << t << "type    : " << ToStringDescriptorType(obj.descriptor_type) << "\n";

		// array
		if (obj.array.dims_count > 0) {
			os << t << "array   : ";
			for (uint32_t dim_index = 0; dim_index < obj.array.dims_count; ++dim_index) {
				os << "[" << obj.array.dims[dim_index] << "]";
			}
			os << "\n";
		}

		// counter
		if (obj.uav_counter_binding != nullptr) {
			os << t << "counter : ";
			os << "(";
			os << "set=" << obj.uav_counter_binding->set << ", ";
			os << "binding=" << obj.uav_counter_binding->binding << ", ";
			os << "name=" << obj.uav_counter_binding->name;
			os << ");";
			os << "\n";
		}

		os << t << "name    : " << obj.name;
		if ((obj.type_description->type_name != nullptr) && (strlen(obj.type_description->type_name) > 0)) {
			os << " " << "(" << obj.type_description->type_name << ")";
		}
	}

	void PrintDescriptorSet(std::ostream& os, const SpvReflectDescriptorSet& obj, const char* indent)
	{
		const char* t = indent;
		std::string tt = std::string(indent) + "  ";
		std::string ttttt = std::string(indent) + "    ";

		os << t << "set           : " << obj.set << "\n";
		os << t << "binding count : " << obj.binding_count;
		os << "\n";
		for (uint32_t i = 0; i < obj.binding_count; ++i) {
			const SpvReflectDescriptorBinding& binding = *obj.bindings[i];
			os << tt << i << ":" << "\n";
			PrintDescriptorBinding(os, binding, false, ttttt.c_str());
			if (i < (obj.binding_count - 1)) {
				os << "\n";
			}
		}
	}

	


	bool VulkanShader::CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint, std::string* oErrorMsgs)
	{
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "CompileShaderFromFile: %s(%s)", *FileName, EntryPoint);

		AssetPath shaderRoot("shaders");
		AssetPath shaderBinObject( (std::string("CACHE\\") + FileName.GetName() + ".SPIRV").c_str());
		AssetPath shaderBuildOutput((std::string("CACHE\\") + FileName.GetName() + ".txt").c_str());

		std::string FullDXCPath = SPP::GRootPath + "3rdParty/dxc/bin/x64/dxc.exe";

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

		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, " - SUCCESS CompileShaderFromFile: %s(%s)", *FileName, EntryPoint);
		_entryPoint = EntryPoint;

		// reflection parsing
		{
			SpvReflectShaderModule module = {};
			SpvReflectResult result = spvReflectCreateShaderModule(FileData.size(), FileData.data(), &module);
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			uint32_t count = 0;
			result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			std::vector<SpvReflectDescriptorSet*> sets(count);
			result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			//std::vector<DescriptorSetLayoutData> set_layouts(sets.size(), DescriptorSetLayoutData{});
			//for (size_t i_set = 0; i_set < sets.size(); ++i_set) {
			//	const SpvReflectDescriptorSet& refl_set = *(sets[i_set]);
			//	DescriptorSetLayoutData& layout = set_layouts[i_set];
			//	layout.bindings.resize(refl_set.binding_count);
			//	for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding) {
			//		const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
			//		VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
			//		layout_binding.binding = refl_binding.binding;
			//		layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
			//		layout_binding.descriptorCount = 1;
			//		for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim) {
			//			layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
			//		}
			//		layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);
			//	}
			//	layout.set_number = refl_set.set;
			//	layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			//	layout.create_info.bindingCount = refl_set.binding_count;
			//	layout.create_info.pBindings = layout.bindings.data();
			//}

			 // Log the descriptor set contents to stdout
			const char* t = "  ";
			const char* tt = "    ";

			PrintModuleInfo(std::cout, module, "");
			std::cout << "\n\n";

			std::cout << "Descriptor sets:" << "\n";
			for (size_t index = 0; index < sets.size(); ++index) {
				auto p_set = sets[index];

				// descriptor sets can also be retrieved directly from the module, by set index
				auto p_set2 = spvReflectGetDescriptorSet(&module, p_set->set, &result);
				assert(result == SPV_REFLECT_RESULT_SUCCESS);
				assert(p_set == p_set2);
				(void)p_set2;

				std::cout << t << index << ":" << "\n";
				PrintDescriptorSet(std::cout, *p_set, tt);
				std::cout << "\n\n";
			}

			spvReflectDestroyShaderModule(&module);
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