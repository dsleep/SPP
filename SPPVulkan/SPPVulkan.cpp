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
		return std::make_shared<VulkanGraphicsDevice>();
	}
	
	struct VulkanGraphicInterface : public IGraphicsInterface
	{
		std::unique_ptr< VulkanGraphicsDevice > graphicsDevice;

		// hacky so one GGI per DLL
		VulkanGraphicInterface()
		{
			SET_GGI(this);
		}

		virtual void CreateGraphicsDevice() override
		{
			graphicsDevice = std::make_unique< VulkanGraphicsDevice >();
		}

		virtual void DestroyraphicsDevice() override
		{
			graphicsDevice.reset();
		}

		virtual GraphicsDevice* GetGraphicsDevice() override
		{
			return graphicsDevice.get();
		}
	};

	static VulkanGraphicInterface staticDGI;
}