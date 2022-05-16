// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "vulkan/vulkan.h"

#include "VulkanFrameBuffer.hpp"
#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "VulkanShaders.h"
#include "VulkanRenderScene.h"
#include "VulkanRenderableSDF.h"

#include "VulkanInitializers.hpp"

#if PLATFORM_WINDOWS
	#include "vulkan/vulkan_win32.h"
#endif

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	LogEntry LOG_VULKAN("Vulkan");

	VkDevice GGlobalVulkanDevice = nullptr;
	VulkanGraphicsDevice* GGlobalVulkanGI = nullptr;

#define FRAME_COUNT 3

	

	std::shared_ptr< GraphicsDevice > Vulkan_CreateGraphicsDevice()
	{
		return std::make_shared< VulkanGraphicsDevice>();
	}

	extern GPUReferencer< GPUShader > Vulkan_CreateShader(EShaderType InType);
	extern GPUReferencer< GPUTexture > Vulkan_CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);
	extern std::shared_ptr<GD_RenderableSignedDistanceField> Vulkan_CreateSDF(GD_RenderableSignedDistanceField::Args&& InArgs);

	struct VulkanGraphicInterface : public IGraphicsInterface
	{
		// hacky so one GGI per DLL
		VulkanGraphicInterface()
		{
			SET_GGI(this);
		}
		virtual GPUReferencer< GPUShader > CreateShader(EShaderType InType) override
		{			
			return Vulkan_CreateShader(InType);
		}
		virtual GPUReferencer< GPUBuffer > CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData = nullptr) override
		{
			return nullptr;
		}
		virtual GPUReferencer< GPUInputLayout > CreateInputLayout() override
		{
			return nullptr;
		}
		virtual GPUReferencer< GPUTexture > CreateTexture(int32_t Width, 
			int32_t Height, 
			TextureFormat Format,
			std::shared_ptr< ArrayResource > RawData = nullptr,
			std::shared_ptr< ImageMeta > InMetaInfo = nullptr) override
		{
			return Vulkan_CreateTexture(Width, Height, Format, RawData, InMetaInfo);
		}
		virtual GPUReferencer< GPURenderTarget > CreateRenderTarget(int32_t Width, int32_t Height, TextureFormat Format) override
		{
			return nullptr;
		}
		virtual std::shared_ptr< GraphicsDevice > CreateGraphicsDevice() override
		{
			return Vulkan_CreateGraphicsDevice();
		}
		virtual std::shared_ptr< GD_ComputeDispatch > CreateComputeDispatch(GPUReferencer< GPUShader> InCS) override
		{
			return nullptr;
		}
		virtual std::shared_ptr<GD_RenderScene> CreateRenderScene() override
		{
			return std::make_shared< VulkanRenderScene >();
		}
		virtual std::shared_ptr<GD_RenderableMesh> CreateRenderableMesh() override
		{
			return nullptr;
		}
		virtual std::shared_ptr<GD_RenderableSignedDistanceField> CreateRenderableSDF(GD_RenderableSignedDistanceField::Args &&InArgs) override
		{
			return Vulkan_CreateSDF(InArgs);
		}
		virtual void BeginResourceCopies() override
		{
			
		}
		virtual void EndResourceCopies() override
		{
			
		}
		virtual bool RegisterMeshElement(std::shared_ptr<struct MeshElement> InMeshElement)
		{
			return true;
		}
		virtual bool UnregisterMeshElement(std::shared_ptr<struct MeshElement> InMeshElement)
		{
			return true;
		}
	};

	static VulkanGraphicInterface staticDGI;
}