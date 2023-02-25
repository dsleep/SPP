// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "VulkanShaders.h"
#include "VulkanTools.h"

#include "SPPLogging.h"
#include "SPPFileSystem.h"
#include "SPPAssetCache.h"

#include "spirvreflect/spirv_reflect.h"
#include "spirvreflect/spirv_output_stream.h"

#include "SPPPlatformCore.h"
#include <thread>
#include "SPPCrypto.h"

namespace SPP
{
	extern VkDevice GGlobalVulkanDevice;

	LogEntry LOG_VULKANSHADER("VulkanShader");

	VkShaderStageFlags ReturnVkShaderStage(EShaderType inType)
	{
		switch (inType)
		{
		case EShaderType::Pixel:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		case EShaderType::Vertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case EShaderType::Compute:
			return VK_SHADER_STAGE_COMPUTE_BIT;
		//case EShaderType::Domain:
		//	return "ds_6_5";
		//case EShaderType::Hull:
		//	return "hs_6_5";
		//case EShaderType::Mesh:
		//	return "ms_6_5";
		//case EShaderType::Amplification:
		//	return "as_6_5";
		}
		return VK_SHADER_STAGE_ALL;
	}

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

	const char* ReturnTargetStringGLSL(EShaderType inType)
	{
		switch (inType)
		{
		case EShaderType::Pixel:
			return "frag";
		case EShaderType::Vertex:
			return "vert";
		case EShaderType::Compute:
			return "comp";
			//TODO these correct?
		case EShaderType::Domain:
			return "tesc";
		case EShaderType::Hull:
			return "tese";
		case EShaderType::Mesh:
			return "geom";
		//case EShaderType::Amplification:
			//return "as_6_5";
		}
		return "none";
	}

	VulkanShader::VulkanShader(EShaderType InType) : GPUShader(InType)
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

	void RecursivelyLogMembers(const SpvReflectBlockVariable& Member, const char* indent)
	{
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%s - %s : %d(%d)", indent, Member.name, Member.size, Member.offset);
	
		for (int32_t Iter = 0; Iter < Member.member_count; Iter++)
		{
			auto& CurMember = Member.members[Iter];
			RecursivelyLogMembers(CurMember, indent);
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
				RecursivelyLogMembers(obj.block.members[Iter], indent);
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
	
	template<>
	inline BinarySerializer& operator<< <VkPushConstantRange> (BinarySerializer& Storage, const VkPushConstantRange& Value)
	{
		//VkShaderStageFlags    stageFlags;
		//uint32_t              offset;
		//uint32_t              size;
		Storage << Value.stageFlags;
		Storage << Value.offset;
		Storage << Value.size;
		return Storage;
	}
	template<>
	inline BinarySerializer& operator>> <VkPushConstantRange> (BinarySerializer& Storage, VkPushConstantRange& Value)
	{
		Storage >> Value.stageFlags;
		Storage >> Value.offset;
		Storage >> Value.size;
		return Storage;
	}

	template<>
	inline BinarySerializer& operator<< <VkDescriptorSetLayoutBinding> (BinarySerializer& Storage, const VkDescriptorSetLayoutBinding& Value)
	{
		//uint32_t              binding;
		//VkDescriptorType      descriptorType;
		//uint32_t              descriptorCount;
		//VkShaderStageFlags    stageFlags;
		//const VkSampler* pImmutableSamplers;
		Storage << Value.binding;
		Storage << Value.descriptorType;
		Storage << Value.descriptorCount;
		Storage << Value.stageFlags;
		return Storage;
	}
	template<>
	inline BinarySerializer& operator>> <VkDescriptorSetLayoutBinding> (BinarySerializer& Storage, VkDescriptorSetLayoutBinding& Value)
	{
		Value = VkDescriptorSetLayoutBinding{ 0 };
		//uint32_t              binding;
		//VkDescriptorType      descriptorType;
		//uint32_t              descriptorCount;
		//VkShaderStageFlags    stageFlags;
		//const VkSampler* pImmutableSamplers;
		Storage >> Value.binding;
		Storage >> Value.descriptorType;
		Storage >> Value.descriptorCount;
		Storage >> Value.stageFlags;
		return Storage;
	}

	template<>
	inline BinarySerializer& operator<< <DescriptorSetLayoutData> (BinarySerializer& Storage, const DescriptorSetLayoutData& Value)
	{
		//uint32_t set_number;
		//std::vector<VkDescriptorSetLayoutBinding> bindings;
		Storage << Value.set_number;
		Storage << Value.bindings;
		return Storage;
	}
	template<>
	inline BinarySerializer& operator>> <DescriptorSetLayoutData> (BinarySerializer& Storage, DescriptorSetLayoutData& Value)
	{
		Storage >> Value.set_number;
		Storage >> Value.bindings;
		return Storage;
	}

	static bool const GUseShaderCached = true;
	static uint32_t const ShaderCacheVersion = 2;

	bool VulkanShader::CompileShaderFromString(const std::string& ShaderSource, const AssetPath& ReferencePath, const char* EntryPoint, std::string* oErrorMsgs) 
	{
		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "CompileShaderFromString: ref path %s(%s)", *ReferencePath, EntryPoint);

		_entryPoint = EntryPoint;

		auto sourceHash = SHA256MemHash(ShaderSource.c_str(), ShaderSource.length());

		if (GUseShaderCached)
		{
			std::shared_ptr<BinaryBlobSerializer> FoundCachedBlob = GetCachedAsset(ReferencePath, sourceHash + EntryPoint);

			if (FoundCachedBlob)
			{
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "CompileShaderFromString: found cache");
				BinaryBlobSerializer& blobAsset = *FoundCachedBlob;

				uint32_t fileVersion = 0;

				blobAsset >> fileVersion;
				if (fileVersion == ShaderCacheVersion)
				{
					std::vector<uint8_t> binShaderData;
					blobAsset >> binShaderData;

					VkShaderModuleCreateInfo moduleCreateInfo{};
					moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
					moduleCreateInfo.codeSize = binShaderData.size();
					moduleCreateInfo.pCode = (uint32_t*)binShaderData.data();

					VkResult results = vkCreateShaderModule(GGlobalVulkanDevice, &moduleCreateInfo, NULL, &_shader);
					if (results == VK_SUCCESS)
					{
						blobAsset >> _layoutSets;
						blobAsset >> _pushConstants;
						return true;
					}
				}
			}
		}

		auto fileNameExt = stdfs::path(ReferencePath.GetName()).extension().generic_string();
		inlineToUpper(fileNameExt);

		bool IsGLSL = (fileNameExt == ".GLSL");

		AssetPath shaderRoot("shaders");
		AssetPath shaderOutputObject( (std::string("CACHE\\") + ReferencePath.GetName() ).c_str());
		AssetPath shaderBinObject( (std::string("CACHE\\") + ReferencePath.GetName() + ".SPIRV").c_str());
		AssetPath shaderBuildOutput( (std::string("CACHE\\") + ReferencePath.GetName() + ".txt").c_str());

		WriteStringToFile(*shaderOutputObject, ShaderSource);
		
		std::string CommandString;
		std::string FullBinPath;

		if (IsGLSL)
		{
			const char* env_p = std::getenv("VULKAN_SDK");
			FullBinPath = std::string(env_p) + "/Bin/glslangValidator.exe";

			CommandString = std::string_format("\"%s\" -V --target-env vulkan1.2 -g -S %s -e %s -I\"%s\" -o \"%s\"",
				*shaderOutputObject,
				ReturnTargetStringGLSL(_type),
				EntryPoint,
				*shaderRoot,
				*shaderBinObject
			);
		}
		else
		{
			FullBinPath = SPP::GRootPath + "3rdParty/dxc/bin/x64/dxc.exe";

			CommandString = std::string_format("\"%s\" -spirv -fspv-debug=line -fspv-reflect -Zpr -Zi -T %s -E %s -I \"%s\" -Fo \"%s\"",
				*shaderOutputObject,
				ReturnTargetString(_type),
				EntryPoint,
				*shaderRoot,
				*shaderBinObject
			);
		}
		{
			auto ShaderProcess = CreatePlatformProcess(FullBinPath.c_str(), CommandString.c_str(), false, true);

			using namespace std::chrono_literals;
			if (ShaderProcess->IsValid())
			{
				auto ProcessOutput = ShaderProcess->GetOutput();

				if (!ProcessOutput.empty())
				{
					SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%s", ProcessOutput.c_str());

					if (ProcessOutput.find("ERROR") != std::string::npos) 
					{
						return false;
					}					
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

		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, " - SUCCESS CompileShaderFromFile: %s(%s)", *ReferencePath, EntryPoint);
		

		// reflection parsing
		{
			spv_reflect::ShaderModule spvReflSM(FileData);
			SpvReflectResult result = spvReflSM.GetResult();
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			SpvReflectShaderModule module = spvReflSM.GetShaderModule();
			{
				std::ostringstream refStream;
				WriteReflection(spvReflSM, false, refStream);
				std::string refStr = refStream.str();
				auto writeRefStrPath = stdfs::path(GetLogPath()).parent_path() / stdfs::path(*ReferencePath).filename();
				writeRefStrPath.replace_extension(".refl.txt");
				
				SPP_LOG(LOG_VULKANSHADER, LOG_INFO, " - writing refl data %s", writeRefStrPath.generic_string().c_str());

				WriteStringToFile(writeRefStrPath.generic_string().c_str(), refStr);
			}

			uint32_t count = 0;

			// push constants
			result = spvReflectEnumeratePushConstantBlocks(&module, &count, NULL);
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			std::vector<SpvReflectBlockVariable*> pushConstants(count);

			result = spvReflectEnumeratePushConstantBlocks(&module, &count, pushConstants.data());
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			SPP_LOG(LOG_VULKANSHADER, LOG_VERBOSE, "PUSH CONSTANTS");
			for (auto& pushvars : pushConstants)
			{
				SPP_LOG(LOG_VULKANSHADER, LOG_VERBOSE, " - %s %d %d", pushvars->name, pushvars->offset, pushvars->size);

				_pushConstants.push_back(
					VkPushConstantRange{
						ReturnVkShaderStage(_type ),
						pushvars->offset,
						pushvars->size }
				);
			}

			// descriptor/uniform reflection
			result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			std::vector<SpvReflectDescriptorSet*> sets(count);
			result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
			assert(result == SPV_REFLECT_RESULT_SUCCESS);

			//{
			//	uint32_t var_count = 0;
			//	result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
			//	assert(result == SPV_REFLECT_RESULT_SUCCESS);
			//	if (var_count > 0)
			//	{
			//		SpvReflectInterfaceVariable** input_vars =
			//			(SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
			//		result = spvReflectEnumerateInputVariables(&module, &var_count, input_vars);
			//		assert(result == SPV_REFLECT_RESULT_SUCCESS);

			//		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "INPUT VARIABLES");

			//		for (uint32_t Iter = 0; Iter < var_count; Iter++)
			//		{
			//			SPP_LOG(LOG_VULKANSHADER, LOG_INFO, " - %s : %d", input_vars[Iter]->name, input_vars[Iter]->location);
			//		}
			//		free(input_vars);
			//	}
			//}

			//{
			//	uint32_t var_count = 0;
			//	result = spvReflectEnumerateOutputVariables(&module, &var_count, NULL);
			//	assert(result == SPV_REFLECT_RESULT_SUCCESS);
			//	if (var_count > 0)
			//	{
			//		SpvReflectInterfaceVariable** reflected_vars =
			//			(SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
			//		result = spvReflectEnumerateOutputVariables(&module, &var_count, reflected_vars);
			//		assert(result == SPV_REFLECT_RESULT_SUCCESS);

			//		SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "OUTPUT VARIABLES");

			//		for (uint32_t Iter = 0; Iter < var_count; Iter++)
			//		{
			//			SPP_LOG(LOG_VULKANSHADER, LOG_INFO, " - %s : %d", reflected_vars[Iter]->name, reflected_vars[Iter]->location);
			//		}
			//		free(reflected_vars);
			//	}
			//}

			//std::vector<DescriptorSetLayoutData> set_layouts(sets.size(), DescriptorSetLayoutData{});

			_layoutSets.clear();

			for (size_t i_set = 0; i_set < sets.size(); ++i_set) 
			{
				const SpvReflectDescriptorSet& refl_set = *(sets[i_set]);

				DescriptorSetLayoutData& layout = _layoutSets.emplace_back();
				//DescriptorSetLayoutData& layout = set_layouts[i_set];
				layout.bindings.resize(refl_set.binding_count);
				for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding) 
				{
					const SpvReflectDescriptorBinding& refl_binding = *(refl_set.bindings[i_binding]);
					VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[i_binding];
					layout_binding.binding = refl_binding.binding;
					layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
				
					layout_binding.descriptorCount = 1;
					for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim) {
						layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
					}
					layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);

					// we only use dynamics to keep it simple
					if (layout_binding.descriptorCount == 1)
					{
						if (layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
						{
							layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
						}
						else if (layout_binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
						{
							layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
						}
					}

					// lets make the graphics pass just share this, problems?
					if (layout_binding.stageFlags & (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT))
					{
						layout_binding.stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
					}

				}
				layout.set_number = refl_set.set;
				//layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				//layout.create_info.bindingCount = refl_set.binding_count;
				//layout.create_info.pBindings = layout.bindings.data();
			}

			//const char* tt = "  ";
			//PrintModuleInfo(module, "");
			//SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "Descriptor sets:");
			//for (size_t index = 0; index < sets.size(); ++index) {
			//	auto p_set = sets[index];

			//	// descriptor sets can also be retrieved directly from the module, by set index
			//	auto p_set2 = spvReflectGetDescriptorSet(&module, p_set->set, &result);
			//	assert(result == SPV_REFLECT_RESULT_SUCCESS);
			//	assert(p_set == p_set2);
			//	(void)p_set2;

			//	SPP_LOG(LOG_VULKANSHADER, LOG_INFO, "%sSET : GPU IDX %d", tt, p_set->set);
			//	PrintDescriptorSet(*p_set, tt);
			//}

			//spvReflectDestroyShaderModule(&module);
		}

		//CACHING
		if (GUseShaderCached)
		{
			// create cache
			BinaryBlobSerializer outCachedAsset;
			outCachedAsset << ShaderCacheVersion;
			outCachedAsset << FileData;
			outCachedAsset << _layoutSets;
			outCachedAsset << _pushConstants;
			PutCachedAsset(ReferencePath, outCachedAsset, sourceHash + EntryPoint);
		}

		return true;
	}
}