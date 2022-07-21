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

	VulkanShader::VulkanShader(GraphicsDevice* InOwner, EShaderType InType) : GPUShader(InOwner,InType)
	{
		SE_ASSERT(InType == EShaderType::Pixel || InType == EShaderType::Vertex || InType == EShaderType::Compute);
	}

	void PrintModuleInfo(const SpvReflectShaderModule& obj, const char* /*indent*/)
	{
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "entry point: %s", obj.entry_point_name);
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "source lang: %s", spvReflectSourceLanguage(obj.source_language));
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "source lang ver: %d", obj.source_language_version);

		if (obj.source_language == SpvSourceLanguageHLSL) {			
			switch (obj.shader_stage) {
			default: break;
			case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "stage: VS");
				break;
			case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "stage: HS");
				break;
			case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: 
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "stage: DS");
				break;
			case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "stage: GS");
				break;
			case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "stage: PS");
				break;
			case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "stage: CS");
				break;
			}
		}
	}

	void RecursivelyLogMembers(const SpvReflectBlockVariable& Member)
	{
		for (int32_t Iter = 0; Iter < Member.member_count; Iter++)
		{
			auto& CurMember = Member.members[Iter];
			SPP_LOG(LOG_VULKANSHADER, LOG_INFO, " - %s : %d", CurMember.name, CurMember.offset);

			RecursivelyLogMembers(CurMember);
		}
	}

	void PrintDescriptorBinding(const SpvReflectDescriptorBinding& obj, bool write_set, const char* indent)
	{		
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%sbinding : %d", indent, obj.binding);
		if (write_set) {
			SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%set : %d", indent, obj.set);
		}		
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%stype : %s", indent, ToStringDescriptorType(obj.descriptor_type).c_str());

		// array
		if (obj.array.dims_count > 0) {
			SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%sarray", indent);

			for (uint32_t dim_index = 0; dim_index < obj.array.dims_count; ++dim_index) {
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%s[%d]", indent, obj.array.dims[dim_index]);
			}			
		}

		// counter
		if (obj.uav_counter_binding != nullptr) {			
			SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%scounter: set=%s, binding=%s, name=%s", 
				indent,
				obj.uav_counter_binding->set,
				obj.uav_counter_binding->binding,
				obj.uav_counter_binding->name );			
		}

		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%sname: %s", indent, obj.name);
		if ((obj.type_description->type_name != nullptr) && (strlen(obj.type_description->type_name) > 0))
		{
			SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%s(%s)", indent, obj.type_description->type_name);

			for(int32_t Iter = 0; Iter < obj.block.member_count; Iter++)			
			{
				RecursivelyLogMembers(obj.block.members[Iter]);
			}
		}
	}

	

	void PrintDescriptorSet(const SpvReflectDescriptorSet& obj, const char* indent)
	{
		const char* t = indent;
		std::string tt = std::string(indent) + "  ";
		std::string ttttt = tt + "  ";
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%sbinding count: %d", t, obj.binding_count);

		for (uint32_t i = 0; i < obj.binding_count; ++i) 
		{
			const SpvReflectDescriptorBinding& binding = *obj.bindings[i];			
			SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%s%d: ", tt.c_str(), i);
			PrintDescriptorBinding(binding, false, ttttt.c_str());			
		}
	}

	


	bool VulkanShader::CompileShaderFromFile(const AssetPath& FileName, const char* EntryPoint, std::string* oErrorMsgs)
	{
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "CompileShaderFromFile: %s(%s)", *FileName, EntryPoint);

		AssetPath shaderRoot("shaders");
		AssetPath shaderBinObject( (std::string("CACHE\\") + FileName.GetName() + ".SPIRV").c_str());
		AssetPath shaderBuildOutput((std::string("CACHE\\") + FileName.GetName() + ".txt").c_str());

		std::string FullDXCPath = SPP::GRootPath + "3rdParty/dxc/bin/x64/dxc.exe";

		std::string CommandString = std::string_format("\"%s\" -spirv -fspv-debug=line -fspv-reflect -Zpr -Zi -T %s -E %s -I \"%s\" -Fo \"%s\"",
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

			{
				uint32_t var_count = 0;
				result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
				assert(result == SPV_REFLECT_RESULT_SUCCESS);
				if (var_count > 0)
				{
					SpvReflectInterfaceVariable** input_vars =
						(SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
					result = spvReflectEnumerateInputVariables(&module, &var_count, input_vars);
					assert(result == SPV_REFLECT_RESULT_SUCCESS);

					SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "INPUT VARIABLES");

					for (uint32_t Iter = 0; Iter < var_count; Iter++)
					{
						SPP_LOG(LOG_VULKANSHADER, LOG_INFO, " - %s : %d", input_vars[Iter]->name, input_vars[Iter]->location);
					}
					free(input_vars);
				}
			}

			{
				uint32_t var_count = 0;
				result = spvReflectEnumerateOutputVariables(&module, &var_count, NULL);
				assert(result == SPV_REFLECT_RESULT_SUCCESS);
				if (var_count > 0)
				{
					SpvReflectInterfaceVariable** reflected_vars =
						(SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
					result = spvReflectEnumerateOutputVariables(&module, &var_count, reflected_vars);
					assert(result == SPV_REFLECT_RESULT_SUCCESS);

					SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "OUTPUT VARIABLES");

					for (uint32_t Iter = 0; Iter < var_count; Iter++)
					{
						SPP_LOG(LOG_VULKANSHADER, LOG_INFO, " - %s : %d", reflected_vars[Iter]->name, reflected_vars[Iter]->location);
					}
					free(reflected_vars);
				}
			}

			//std::vector<DescriptorSetLayoutData> set_layouts(sets.size(), DescriptorSetLayoutData{});

			_layoutSets.clear();

			for (size_t i_set = 0; i_set < sets.size(); ++i_set) 
			{
				const SpvReflectDescriptorSet& refl_set = *(sets[i_set]);

				DescriptorSetLayoutData& layout = _layoutSets.emplace_back();
				//DescriptorSetLayoutData& layout = set_layouts[i_set];
				layout.bindings.resize(refl_set.binding_count);
				for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding) {
					const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
					VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
					layout_binding.binding = refl_binding.binding;
					layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
					layout_binding.descriptorCount = 1;
					for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim) {
						layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
					}
					layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);
				}
				layout.set_number = refl_set.set;
				//layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				//layout.create_info.bindingCount = refl_set.binding_count;
				//layout.create_info.pBindings = layout.bindings.data();
			}

			const char* tt = "  ";
			PrintModuleInfo(module, "");
			SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "Descriptor sets:");
			for (size_t index = 0; index < sets.size(); ++index) {
				auto p_set = sets[index];

				// descriptor sets can also be retrieved directly from the module, by set index
				auto p_set2 = spvReflectGetDescriptorSet(&module, p_set->set, &result);
				assert(result == SPV_REFLECT_RESULT_SUCCESS);
				assert(p_set == p_set2);
				(void)p_set2;

				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%sSET : GPU IDX %d", tt, p_set->set);
				PrintDescriptorSet(*p_set, tt);
			}

			spvReflectDestroyShaderModule(&module);
		}

		return true;
	}
	
	bool VulkanShader::CompileShaderFromString(const std::string& ShaderSource, const char* ShaderName, const char* EntryPoint, std::string* oErrorMsgs)
	{
		return false;
	}

	GPUReferencer< GPUShader > Vulkan_CreateShader(GraphicsDevice* InOwner, EShaderType InType)
	{
		return Make_GPU< VulkanShader >(InOwner, InType);
	}
}