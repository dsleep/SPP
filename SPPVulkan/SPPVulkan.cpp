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

#define FRAME_COUNT 3

	class VulkanGraphicsDevice : public GraphicsDevice
	{
	private:

		uint32_t width = 1280;
		uint32_t height = 720;

		// Vulkan instance, stores all per-application states
		VkInstance instance = nullptr;
		std::vector<std::string> supportedInstanceExtensions;
		// Physical device (GPU) that Vulkan will use
		VkPhysicalDevice physicalDevice = nullptr;
		// Stores physical device properties (for e.g. checking device limits)
		VkPhysicalDeviceProperties deviceProperties{ 0 };
		// Stores the features available on the selected physical device (for e.g. checking if a feature is available)
		VkPhysicalDeviceFeatures deviceFeatures{ 0 };
		// Stores all available memory (type) properties for the physical device
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties;

		/** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
		VkPhysicalDeviceFeatures enabledFeatures{};
		/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
		std::vector<const char*> enabledDeviceExtensions;
		std::vector<const char*> enabledInstanceExtensions;
		/** @brief Optional pNext structure for passing extension structures to device creation */
		void* deviceCreatepNextChain = nullptr;
		/** @brief Logical device, application's view of the physical device (GPU) */
		VkDevice device = nullptr;
		// Handle to the device graphics queue that command buffers are submitted to
		VkQueue queue;
		// Depth buffer format (selected during Vulkan initialization)
		VkFormat depthFormat;
		// Command buffer pool
		VkCommandPool cmdPool;
		/** @brief Pipeline stages used to wait at for graphics queue submissions */
		VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// Contains command buffers and semaphores to be presented to the queue
		VkSubmitInfo submitInfo;
		// Command buffers used for rendering
		std::vector<VkCommandBuffer> drawCmdBuffers;
		// Global render pass for frame buffer writes
		VkRenderPass renderPass = VK_NULL_HANDLE;
		// List of available frame buffers (same as number of swap chain images)
		std::vector<VkFramebuffer>frameBuffers;


		// Active frame buffer index
		uint32_t currentBuffer = 0;
		// Descriptor set pool
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		// List of shader modules created (stored for cleanup)
		std::vector<VkShaderModule> shaderModules;
		// Pipeline cache object
		VkPipelineCache pipelineCache;
		// Wraps the swap chain to present images (framebuffers) to the windowing system
		VulkanSwapChain swapChain;
		// Synchronization semaphores
		struct {
			// Swap chain image presentation
			VkSemaphore presentComplete;
			// Command buffer submission and execution
			VkSemaphore renderComplete;
		} semaphores;
		std::vector<VkFence> waitFences;

		struct {
			VkImage image;
			VkDeviceMemory mem;
			VkImageView view;
		} depthStencil;

		/** @brief Encapsulated physical and logical vulkan device */
		vks::VulkanDevice* vulkanDevice;

		/** @brief Example settings that can be changed e.g. by command line arguments */
		struct Settings {
			/** @brief Activates validation layers (and message output) when set to true */
			bool validation = false;
			/** @brief Set to true if fullscreen mode has been requested via command line */
			bool fullscreen = false;
			/** @brief Set to true if v-sync will be forced for the swapchain */
			bool vsync = false;
			/** @brief Enable UI overlay */
			bool overlay = true;
		} settings;

#if PLATFORM_WINDOWS
		HWND window;
		HINSTANCE windowInstance;
#endif

		

		VkResult createInstance(bool enableValidation)
		{
			settings.validation = enableValidation;

			VkApplicationInfo appInfo = {};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "SPP";
			appInfo.pEngineName = "SPP";
			appInfo.apiVersion = 0;
			std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

			// Enable surface extensions depending on os
#if defined(_WIN32)
			instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
			instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_DIRECT2DISPLAY)
			instanceExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
			instanceExtensions.push_back(VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
			instanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
			instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
			instanceExtensions.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
			instanceExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_HEADLESS_EXT)
			instanceExtensions.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
#endif

			// Get extensions supported by the instance and store for later use
			uint32_t extCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
			if (extCount > 0)
			{
				std::vector<VkExtensionProperties> extensions(extCount);
				if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
				{
					for (VkExtensionProperties extension : extensions)
					{
						supportedInstanceExtensions.push_back(extension.extensionName);
					}
				}
			}

			// Enabled requested instance extensions
			if (enabledInstanceExtensions.size() > 0)
			{
				for (const char* enabledExtension : enabledInstanceExtensions)
				{
					// Output message if requested extension is not available
					if (std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), enabledExtension) == supportedInstanceExtensions.end())
					{
						SPP_LOG(LOG_VULKAN, LOG_INFO, "Enabled instance extension %s is not present at instance level", enabledExtension);
					}
					instanceExtensions.push_back(enabledExtension);
				}
			}

			VkInstanceCreateInfo instanceCreateInfo = {};
			instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instanceCreateInfo.pNext = NULL;
			instanceCreateInfo.pApplicationInfo = &appInfo;
			if (instanceExtensions.size() > 0)
			{
				if (settings.validation)
				{
					instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				}
				instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
				instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
			}

			// The VK_LAYER_KHRONOS_validation contains all current validation functionality.
			// Note that on Android this layer requires at least NDK r20
			const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
			if (settings.validation)
			{
				// Check if this layer is available at instance level
				uint32_t instanceLayerCount;
				vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
				std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
				vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
				bool validationLayerPresent = false;
				for (VkLayerProperties layer : instanceLayerProperties) {
					if (strcmp(layer.layerName, validationLayerName) == 0) {
						validationLayerPresent = true;
						break;
					}
				}
				if (validationLayerPresent) {
					instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
					instanceCreateInfo.enabledLayerCount = 1;
				}
				else {
					SPP_LOG(LOG_VULKAN, LOG_INFO, "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled");
				}
			}
			return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
		}

		bool DeviceInitialize()
		{
			VkResult err;

#if _DEBUG
			settings.validation = true;
#endif

			// Vulkan instance
			err = createInstance(settings.validation);
			if (err) {
				SPP_LOG(LOG_VULKAN, LOG_ERROR, "Could not create Vulkan instance %s", vks::tools::errorString(err));
				return false;
			}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
			vks::android::loadVulkanFunctions(instance);
#endif

			// If requested, we enable the default validation layers for debugging
			if (settings.validation)
			{
				// The report flags determine what type of messages for the layers will be displayed
				// For validating (debugging) an application the error and warning bits should suffice
				VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
				// Additional flags include performance info, loader and layer debug messages, etc.
				vks::debug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
			}

			// Physical device
			uint32_t gpuCount = 0;
			// Get number of available physical devices
			VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
			if (gpuCount == 0) {
				SPP_LOG(LOG_VULKAN, LOG_ERROR, "No device with Vulkan support found");
				return false;
			}
			// Enumerate devices
			std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
			err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());
			if (err) {
				SPP_LOG(LOG_VULKAN, LOG_ERROR, "Could not enumerate physical devices %s", vks::tools::errorString(err));
				return false;
			}
			else
			{
				SPP_LOG(LOG_VULKAN, LOG_ERROR, "Vulkan found %d devices", gpuCount);
			}
			// GPU selection

			// Select physical device to be used for the Vulkan example
			// Defaults to the first device unless specified by command line
			uint32_t selectedDevice = 0;

#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
			// GPU selection via command line argument
			//if (commandLineParser.isSet("gpuselection")) {
			//	uint32_t index = commandLineParser.getValueAsInt("gpuselection", 0);
			//	if (index > gpuCount - 1) {
			//		std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)" << "\n";
			//	}
			//	else {
			//		selectedDevice = index;
			//	}
			//}
			//if (commandLineParser.isSet("gpulist")) {
			SPP_LOG(LOG_VULKAN, LOG_INFO, "Available Vulkan devices");
			for (uint32_t i = 0; i < gpuCount; i++) {
				VkPhysicalDeviceProperties deviceProperties;
				vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
				SPP_LOG(LOG_VULKAN, LOG_INFO, "Device [%d] : %s", i, deviceProperties.deviceName);
				SPP_LOG(LOG_VULKAN, LOG_INFO, " - Type: %s", vks::tools::physicalDeviceTypeString(deviceProperties.deviceType).c_str());
				SPP_LOG(LOG_VULKAN, LOG_INFO, " - API: %d.%d.%d", (deviceProperties.apiVersion >> 22), ((deviceProperties.apiVersion >> 12) & 0x3ff), (deviceProperties.apiVersion & 0xfff));
			}
			//}
#endif

			physicalDevice = physicalDevices[selectedDevice];

			// Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
			vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
			vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

			VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures{};
			indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
			indexingFeatures.pNext = nullptr;

			VkPhysicalDeviceFeatures2 deviceFeatures{};
			deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			deviceFeatures.pNext = &indexingFeatures;
			vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

			if (indexingFeatures.descriptorBindingPartiallyBound && indexingFeatures.runtimeDescriptorArray)
			{
				// all set to use unbound arrays of textures
				SPP_LOG(LOG_VULKAN, LOG_INFO, "BOUNDLESS SUPPORT!");
			}

			// [POI] Enable required extensions
			enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
			enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
			enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

			// [POI] Enable required extension features
			VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeatures{};

			physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
			physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
			physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
			physicalDeviceDescriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;

			//physicalDeviceDescriptorIndexingFeatures.pNext = &otherone

			deviceCreatepNextChain = &physicalDeviceDescriptorIndexingFeatures;

			// Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
			//getEnabledFeatures();

			// Vulkan device creation
			// This is handled by a separate class that gets a logical device representation
			// and encapsulates functions related to a device
			vulkanDevice = new vks::VulkanDevice(physicalDevice);
			VkResult res = vulkanDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain);
			if (res != VK_SUCCESS) {
				SPP_LOG(LOG_VULKAN, LOG_ERROR, "Could not create Vulkan device: %s %d", vks::tools::errorString(res), res);
				return false;
			}
			device = vulkanDevice->logicalDevice;

			GGlobalVulkanDevice = device;

			swapChain.connect(instance, physicalDevice, device);

			// Get a graphics queue from the device
			vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

			// Find a suitable depth format
			VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
			assert(validDepthFormat);

			// Create synchronization objects
			VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
			// Create a semaphore used to synchronize image presentation
			// Ensures that the image is displayed before we start submitting new commands to the queue
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
			// Create a semaphore used to synchronize command submission
			// Ensures that the image is not presented until all commands have been submitted and executed
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));

			// Set up submit info structure
			// Semaphores will stay the same during application lifetime
			// Command buffer submission info is set by each example
			submitInfo = vks::initializers::submitInfo();
			submitInfo.pWaitDstStageMask = &submitPipelineStages;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &semaphores.presentComplete;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &semaphores.renderComplete;

			return true;
		}

		void nextFrame();
		void updateOverlay();
		
		void createPipelineCache()
		{
			VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
			pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
		}
		
		void createCommandPool()
		{
			VkCommandPoolCreateInfo cmdPoolInfo = {};
			cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
			cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
		}

		void createSynchronizationPrimitives()
		{
			// Wait fences to sync command buffer access
			VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
			waitFences.resize(drawCmdBuffers.size());
			for (auto& fence : waitFences) {
				VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
			}
		}

		void initSwapchain()
		{
#if defined(_WIN32)
			swapChain.initSurface(windowInstance, window);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
			swapChain.initSurface(androidApp->window);
#elif (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))
			swapChain.initSurface(view);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
			swapChain.initSurface(dfb, surface);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
			swapChain.initSurface(display, surface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
			swapChain.initSurface(connection, window);
#elif (defined(_DIRECT2DISPLAY) || defined(VK_USE_PLATFORM_HEADLESS_EXT))
			swapChain.initSurface(width, height);
#endif
		}

		void setupSwapChain() 
		{
			swapChain.create(&width, &height, settings.vsync);
		}

		void createCommandBuffers()
		{
			// Create one command buffer for each swap chain image and reuse for rendering
			drawCmdBuffers.resize(swapChain.imageCount);

			VkCommandBufferAllocateInfo cmdBufAllocateInfo =
				vks::initializers::commandBufferAllocateInfo(
					cmdPool,
					VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					static_cast<uint32_t>(drawCmdBuffers.size()));

			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
		}

		void destroyCommandBuffers()
		{
			vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
		}

		void setupDepthStencil()
		{
			VkImageCreateInfo imageCI{};
			imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.format = depthFormat;
			imageCI.extent = { width, height, 1 };
			imageCI.mipLevels = 1;
			imageCI.arrayLayers = 1;
			imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));
			VkMemoryRequirements memReqs{};
			vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

			VkMemoryAllocateInfo memAllloc{};
			memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memAllloc.allocationSize = memReqs.size;
			memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem));
			VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

			VkImageViewCreateInfo imageViewCI{};
			imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCI.image = depthStencil.image;
			imageViewCI.format = depthFormat;
			imageViewCI.subresourceRange.baseMipLevel = 0;
			imageViewCI.subresourceRange.levelCount = 1;
			imageViewCI.subresourceRange.baseArrayLayer = 0;
			imageViewCI.subresourceRange.layerCount = 1;
			imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
			if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
				imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
			VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view));
		}

		void setupRenderPass()
		{
			std::array<VkAttachmentDescription, 2> attachments = {};
			// Color attachment
			attachments[0].format = swapChain.colorFormat;
			attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			// Depth attachment
			attachments[1].format = depthFormat;
			attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference colorReference = {};
			colorReference.attachment = 0;
			colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depthReference = {};
			depthReference.attachment = 1;
			depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpassDescription = {};
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorReference;
			subpassDescription.pDepthStencilAttachment = &depthReference;
			subpassDescription.inputAttachmentCount = 0;
			subpassDescription.pInputAttachments = nullptr;
			subpassDescription.preserveAttachmentCount = 0;
			subpassDescription.pPreserveAttachments = nullptr;
			subpassDescription.pResolveAttachments = nullptr;

			// Subpass dependencies for layout transitions
			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			renderPassInfo.pAttachments = attachments.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpassDescription;
			renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
			renderPassInfo.pDependencies = dependencies.data();

			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
		}


		void setupFrameBuffer()
		{
			VkImageView attachments[2];

			// Depth/Stencil attachment is the same for all frame buffers
			attachments[1] = depthStencil.view;

			VkFramebufferCreateInfo frameBufferCreateInfo = {};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.pNext = NULL;
			frameBufferCreateInfo.renderPass = renderPass;
			frameBufferCreateInfo.attachmentCount = 2;
			frameBufferCreateInfo.pAttachments = attachments;
			frameBufferCreateInfo.width = width;
			frameBufferCreateInfo.height = height;
			frameBufferCreateInfo.layers = 1;

			// Create frame buffers for every swap chain image
			frameBuffers.resize(swapChain.imageCount);
			for (uint32_t i = 0; i < frameBuffers.size(); i++)
			{
				attachments[0] = swapChain.buffers[i].view;
				VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
			}
		}

	public:

		VkDevice GetDevice()
		{
			return device;
		}

		VkRenderPass GetBaseRenderPass()
		{
			return renderPass;
		}

		virtual void Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow)
		{
#if PLATFORM_WINDOWS
			window = (HWND)OSWindow;
			windowInstance = GetModuleHandle(NULL);
#endif

			DeviceInitialize();			
			initSwapchain();
			width = InitialWidth;
			height = InitialHeight;

			cmdPool = vulkanDevice->createCommandPool(swapChain.queueNodeIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
			setupSwapChain();
			createCommandBuffers();
			createSynchronizationPrimitives();

			setupDepthStencil();
			setupRenderPass();
			setupFrameBuffer();
		}

		virtual void ResizeBuffers(int32_t NewWidth, int32_t NewHeight)
		{

		}

		virtual int32_t GetDeviceWidth() const
		{
			return width;
		}
		virtual int32_t GetDeviceHeight() const
		{
			return height;
		}

		void CreateCommonDescriptors()
		{
			//#define MESH_SIG \
			//	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_GEOMETRY_SHADER_ROOT_ACCESS), " \
			//1:		"CBV(b0, visibility=SHADER_VISIBILITY_ALL )," \
			//2:		"CBV(b1, space = 0, visibility = SHADER_VISIBILITY_ALL )," \
			//3:		"CBV(b1, space = 1, visibility = SHADER_VISIBILITY_PIXEL ), " \
			//4:		"CBV(b2, space = 1, visibility = SHADER_VISIBILITY_PIXEL )," \
			//5:		"CBV(b1, space = 2, visibility = SHADER_VISIBILITY_DOMAIN ), " \
			//6:		"CBV(b1, space = 3, visibility = SHADER_VISIBILITY_MESH), " \
			//7:		"RootConstants(b3, num32bitconstants=5), " \
			//8:		"DescriptorTable( SRV(t0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			//9:		"DescriptorTable( SRV(t0, space = 1, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			//10:		"DescriptorTable( SRV(t0, space = 2, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			//11:		"DescriptorTable( SRV(t0, space = 3, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			//12:		"DescriptorTable( SRV(t0, space = 4, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			//13:		"DescriptorTable( Sampler(s0, numDescriptors = 32, flags = DESCRIPTORS_VOLATILE) )"

			//typedef struct VkDescriptorSetLayoutBinding {
			//	uint32_t              binding;
			//	VkDescriptorType      descriptorType;
			//	uint32_t              descriptorCount;
			//	VkShaderStageFlags    stageFlags;
			//	const VkSampler* pImmutableSamplers;
			//} VkDescriptorSetLayoutBinding;

			// uniform buffer
			std::array<VkDescriptorSetLayoutBinding, 13> setLayoutBindings{};
	

			setLayoutBindings[0] = { 0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_ALL };
			setLayoutBindings[1] = { 0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_ALL };
			setLayoutBindings[2] = { 0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_ALL };


			setLayoutBindings[3] = { 1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT };
			

			// Create the descriptor set layout
			VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
			descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
			descriptorLayoutCI.pBindings = setLayoutBindings.data();

			//VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayout));

		}

		void Descriptor()
		{
			//VkDescriptorPoolSize poolSize{};
			//poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			//poolSize.descriptorCount = static_cast<uint32_t>(swapChainImages.size());

			//VkDescriptorPoolCreateInfo poolInfo{};
			//poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			//poolInfo.poolSizeCount = 1;
			//poolInfo.pPoolSizes = &poolSize;

			//poolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());

			////#define MESH_SIG \
			////	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_GEOMETRY_SHADER_ROOT_ACCESS), " \
			////		"CBV(b0, visibility=SHADER_VISIBILITY_ALL )," \
			////		"CBV(b1, space = 0, visibility = SHADER_VISIBILITY_ALL )," \
			////		"CBV(b1, space = 1, visibility = SHADER_VISIBILITY_PIXEL ), " \
			////		"CBV(b2, space = 1, visibility = SHADER_VISIBILITY_PIXEL )," \
			////		"CBV(b1, space = 2, visibility = SHADER_VISIBILITY_DOMAIN ), " \
			////		"CBV(b1, space = 3, visibility = SHADER_VISIBILITY_MESH), " \
			////		"RootConstants(b3, num32bitconstants=5), " \
			////		"DescriptorTable( SRV(t0, space = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			////		"DescriptorTable( SRV(t0, space = 1, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			////		"DescriptorTable( SRV(t0, space = 2, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			////		"DescriptorTable( SRV(t0, space = 3, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			////		"DescriptorTable( SRV(t0, space = 4, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE) )," \
			////		"DescriptorTable( Sampler(s0, numDescriptors = 32, flags = DESCRIPTORS_VOLATILE) )"

			////VK_SHADER_STAGE_VERTEX_BIT = 0x00000001,
			////VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT = 0x00000002,
			////VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT = 0x00000004,
			////VK_SHADER_STAGE_GEOMETRY_BIT = 0x00000008,
			////VK_SHADER_STAGE_FRAGMENT_BIT = 0x00000010,
			////VK_SHADER_STAGE_COMPUTE_BIT = 0x00000020,
			////VK_SHADER_STAGE_ALL_GRAPHICS = 0x0000001F,

			//// Binding 0: Uniform buffer (Vertex shader)
			//VkDescriptorSetLayoutBinding layoutBinding = {};
			//layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			//layoutBinding.descriptorCount = 1;
			//layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			//layoutBinding.pImmutableSamplers = nullptr;

			////we start from just the default empty pipeline layout info
			//VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 
			//	sizeof(PushConstBlock), 0);

			////setup push constants
			//VkPushConstantRange push_constant;
			////this push constant range starts at the beginning
			//push_constant.offset = 0;
			////this push constant range takes up the size of a MeshPushConstants struct
			//push_constant.size = sizeof(MeshPushConstants);
			////this push constant range is accessible only in the vertex shader
			//push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

			//mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
			//mesh_pipeline_layout_info.pushConstantRangeCount = 1;

			//VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &_meshPipelineLayout));

		}

		
		void CreateInputLayout(GPUReferencer < GPUInputLayout > InLayout)
		{
		
		}

		

		


		virtual void BeginFrame()
		{
			// Acquire the next image from the swap chain
			VkResult result = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);
			// Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
			if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
				//windowResize();
			}
			else {
				VK_CHECK_RESULT(result);
			}

			// Use a fence to wait until the command buffer has finished execution before using it again
			VK_CHECK_RESULT(vkWaitForFences(device, 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX));
			VK_CHECK_RESULT(vkResetFences(device, 1, &waitFences[currentBuffer]));

			VkCommandBufferBeginInfo cmdBufInfo = {};
			cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cmdBufInfo.pNext = nullptr;

			// Set clear values for all framebuffer attachments with loadOp set to clear
			// We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
			VkClearValue clearValues[2];
			clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
			clearValues[1].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.pNext = nullptr;
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[currentBuffer];


			auto& commandBuffer = drawCmdBuffers[currentBuffer];

			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

			// Start the first sub pass specified in our default render pass setup by the base class
			// This will clear the color and depth attachment
			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			// Update dynamic viewport state
			VkViewport viewport = {};
			viewport.height = (float)height;
			viewport.width = (float)width;
			viewport.minDepth = (float)0.0f;
			viewport.maxDepth = (float)1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			// Update dynamic scissor state
			VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			//// Bind descriptor sets describing shader binding points
			//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			//// Bind the rendering pipeline
			//// The pipeline (state object) contains all states of the rendering pipeline, binding it will set all the states specified at pipeline creation time
			//vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			//// Bind triangle vertex buffer (contains position and colors)
			//VkDeviceSize offsets[1] = { 0 };
			//vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertices.buffer, offsets);

			//// Bind triangle index buffer
			//vkCmdBindIndexBuffer(drawCmdBuffers[i], indices.buffer, 0, VK_INDEX_TYPE_UINT32);

			//// Draw indexed triangle
			//vkCmdDrawIndexed(drawCmdBuffers[i], indices.count, 1, 0, 0, 1);

			
		}
		virtual void EndFrame()
		{
			auto& commandBuffer = drawCmdBuffers[currentBuffer];
			vkCmdEndRenderPass(commandBuffer);

			// Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
			// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

			// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
			VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			// The submit info structure specifies a command buffer queue submission batch
			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.pWaitDstStageMask = &waitStageMask;               // Pointer to the list of pipeline stages that the semaphore waits will occur at
			submitInfo.pWaitSemaphores = &semaphores.presentComplete;      // Semaphore(s) to wait upon before the submitted command buffer starts executing
			submitInfo.waitSemaphoreCount = 1;                           // One wait semaphore
			submitInfo.pSignalSemaphores = &semaphores.renderComplete;     // Semaphore(s) to be signaled when command buffers have completed
			submitInfo.signalSemaphoreCount = 1;                         // One signal semaphore
			submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer]; // Command buffers(s) to execute in this batch (submission)
			submitInfo.commandBufferCount = 1;                           // One command buffer

			// Submit to the graphics queue passing a wait fence
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, waitFences[currentBuffer]));

			VkResult result = swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete);
			if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
				if (result == VK_ERROR_OUT_OF_DATE_KHR) {
					// Swap chain is no longer compatible with the surface and needs to be recreated
					//windowResize();
					return;
				}
				else {
					VK_CHECK_RESULT(result);
				}
			}
			//VK_CHECK_RESULT(vkQueueWaitIdle(queue));
		}
		virtual void MoveToNextFrame() 
		{
		};
	};

	std::shared_ptr< GraphicsDevice > Vulkan_CreateGraphicsDevice()
	{
		return std::make_shared< VulkanGraphicsDevice>();
	}

	extern GPUReferencer< GPUShader > Vulkan_CreateShader(EShaderType InType);
	extern GPUReferencer< GPUTexture > Vulkan_CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);

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
		virtual std::shared_ptr< ComputeDispatch > CreateComputeDispatch(GPUReferencer< GPUShader> InCS) override
		{
			return nullptr;
		}
		virtual std::shared_ptr<RenderScene> CreateRenderScene() override
		{
			return nullptr;
		}
		virtual std::shared_ptr<RenderableMesh> CreateRenderableMesh() override
		{
			return nullptr;
		}
		virtual std::shared_ptr<RenderableSignedDistanceField> CreateRenderableSDF() override
		{
			return nullptr;
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

	

	class VulkanInputLayout : public GPUInputLayout
	{
	private:
		std::vector<VkVertexInputAttributeDescription> _vertexInputAttributes;
		VkPipelineVertexInputStateCreateInfo _vertexInputState;

	public:
		VkPipelineVertexInputStateCreateInfo& GetVertexInputState()
		{
			return _vertexInputState;
		}
		virtual void UploadToGpu() override {}
		virtual ~VulkanInputLayout() {};

		std::vector< VkVertexInputBindingDescription > InputBindings;
		std::vector< VkVertexInputAttributeDescription > InputAttributes;

		template <typename T, typename... Args>
		void AddVertexAttribute(uintptr_t baseAddr, const T &first, const Args&... args)
		{
			VkVertexInputAttributeDescription Desc{};

			if constexpr (std::is_same_v<T, Vector3>)
			{
				Desc.format = VK_FORMAT_R32G32B32_SFLOAT;
			}
			else if constexpr (std::is_same_v<T, Vector2>)
			{
				Desc.format = VK_FORMAT_R32G32_SFLOAT;
			}
			else if constexpr (std::is_same_v<T, uint32_t>)
			{
				Desc.format = VK_FORMAT_R32_UINT;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				Desc.format = VK_FORMAT_R32_SFLOAT;
			}
			else
			{
				struct DummyType {};
				static_assert(std::is_same_v<T, DummyType>, "AddVertexAttribute: Unknown type");
			}

			Desc.binding = (uint32_t) InputBindings.size();
			Desc.location = (uint32_t) InputAttributes.size();
			Desc.offset = (uint32_t) ( reinterpret_cast<uintptr_t>(&first) - baseAddr );

			// todo real proper check
			SE_ASSERT(Desc.offset < 128);

			InputAttributes.push_back(Desc);
				 
			if constexpr (sizeof...(args) >= 1)
			{
				AddVertexAttribute(baseAddr, args...);
			}
		}

		template<class VertexType, typename... VertexMembers>
		void AddVertexStream(const VertexType &InType, const VertexMembers&... inMembers)
		{
			VkVertexInputBindingDescription vertexInputBinding = {};
			vertexInputBinding.binding = (uint32_t)InputBindings.size();
			vertexInputBinding.stride = sizeof(VertexType);
			vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			AddVertexAttribute(reinterpret_cast<uintptr_t>(&InType), inMembers...);

			InputBindings.push_back(vertexInputBinding);
		}

		void Finalize()
		{
			// Vertex input state used for pipeline creation
			_vertexInputState = {};
			_vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			_vertexInputState.vertexBindingDescriptionCount = (uint32_t)InputBindings.size();
			_vertexInputState.pVertexBindingDescriptions = InputBindings.data();
			_vertexInputState.vertexAttributeDescriptionCount = (uint32_t)InputAttributes.size();
			_vertexInputState.pVertexAttributeDescriptions = InputAttributes.data();
		}

		struct testVErtex
		{
			float yoyo;
			Vector3 thisValue;
		};
		virtual void InitializeLayout(const std::vector< InputLayoutElement>& eleList) override
		{
			testVErtex newVertex;
			AddVertexStream(newVertex, newVertex.yoyo, newVertex.thisValue);

			_vertexInputAttributes.clear();

			uint32_t location = 0;
			for (auto& curele : eleList)
			{
				VkVertexInputAttributeDescription Desc{};

				switch (curele.Type)
				{
				case InputLayoutElementType::Float3:
					Desc.format = VK_FORMAT_R32G32B32_SFLOAT;
					break;
				case InputLayoutElementType::Float2:
					Desc.format = VK_FORMAT_R32G32_SFLOAT;
					break;
				case InputLayoutElementType::UInt:
					Desc.format = VK_FORMAT_R32_UINT;
					break;
				}

				Desc.binding = 0;
				Desc.location = ++location;
				Desc.offset = curele.Offset;

				_vertexInputAttributes.push_back(Desc);
			}

			VkVertexInputBindingDescription vertexInputBinding = {};
			vertexInputBinding.binding = 0;
			//vertexInputBinding.stride = sizeof(Vertex);
			vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


			// Vertex input state used for pipeline creation
			_vertexInputState = {};
			_vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			_vertexInputState.vertexBindingDescriptionCount = 1;
			_vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
			_vertexInputState.vertexAttributeDescriptionCount = _vertexInputAttributes.size();
			_vertexInputState.pVertexAttributeDescriptions = _vertexInputAttributes.data();
		}
	};
	class VulkanPipelineState : public PipelineState
	{
	private:
		// The pipeline layout is used by a pipeline to access the descriptor sets
		// It defines interface (without binding any actual data) between the shader stages used by the pipeline and the shader resources
		// A pipeline layout can be shared among multiple pipelines as long as their interfaces match
		VkPipelineLayout _pipelineLayout;

		// The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
		// Like the pipeline layout it's pretty much a blueprint and can be used with different descriptor sets as long as their layout matches
		VkDescriptorSetLayout _descriptorSetLayout;

		// Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
		// While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
		// So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
		// Even though this adds a new dimension of planing ahead, it's a great opportunity for performance optimizations by the driver
		VkPipeline _pipeline;

		VulkanGraphicsDevice* _parent = nullptr;

	public:
		VulkanPipelineState(VulkanGraphicsDevice* InParent) : _parent(InParent)
		{

		}
		virtual ~VulkanPipelineState() {};

		void Initialize(EBlendState InBlendState,
			ERasterizerState InRasterizerState,
			EDepthState InDepthState,
			EDrawingTopology InTopology,
			GPUReferencer < GPUInputLayout > InLayout,
			GPUReferencer< GPUShader> InVS,
			GPUReferencer< GPUShader> InPS,

			GPUReferencer< GPUShader> InMS,
			GPUReferencer< GPUShader> InAS,
			GPUReferencer< GPUShader> InHS,
			GPUReferencer< GPUShader> InDS,

			GPUReferencer< GPUShader> InCS)
		{
			auto device = _parent->GetDevice();
			auto renderPass = _parent->GetBaseRenderPass();
			auto inputLayout = InLayout->GetAs<VulkanInputLayout>();

			// Setup layout of descriptors used in this example
			// Basically connects the different shader stages to descriptors for binding uniform buffers, image samplers, etc.
			// So every shader binding should map to one descriptor set layout binding

			// Binding 0: Uniform buffer (Vertex shader)
			VkDescriptorSetLayoutBinding layoutBinding = {};
			layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			layoutBinding.descriptorCount = 1;
			layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			layoutBinding.pImmutableSamplers = nullptr;

			VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
			descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorLayout.pNext = nullptr;
			descriptorLayout.bindingCount = 1;
			descriptorLayout.pBindings = &layoutBinding;

			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &_descriptorSetLayout));

			// Create the pipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
			// In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused
			VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
			pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pPipelineLayoutCreateInfo.pNext = nullptr;
			pPipelineLayoutCreateInfo.setLayoutCount = 1;
			pPipelineLayoutCreateInfo.pSetLayouts = &_descriptorSetLayout;

			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &_pipelineLayout));

			// Vulkan uses the concept of rendering pipelines to encapsulate fixed states, replacing OpenGL's complex state machine
			// A pipeline is then stored and hashed on the GPU making pipeline changes very fast
			// Note: There are still a few dynamic states that are not directly part of the pipeline (but the info that they are used is)

			VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			// The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
			pipelineCreateInfo.layout = _pipelineLayout;
			// Renderpass this pipeline is attached to
			pipelineCreateInfo.renderPass = renderPass;

			// Construct the different states making up the pipeline

			// Input assembly state describes how primitives are assembled
			// This pipeline will assemble vertex data as a triangle lists (though we only use one triangle)
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
			inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

			switch (InTopology)
			{
			case EDrawingTopology::TriangleList:
				inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				break;
			case EDrawingTopology::TriangleStrip:
				inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				break;
			case EDrawingTopology::LineList:
				inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
				break;
			case EDrawingTopology::PointList:
				inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
				break;
			default:
				SE_ASSERT(false);
			}

			// Rasterization state
			VkPipelineRasterizationStateCreateInfo rasterizationState = {};
			rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizationState.cullMode = VK_CULL_MODE_NONE;
			rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizationState.depthClampEnable = VK_FALSE;
			rasterizationState.rasterizerDiscardEnable = VK_FALSE;
			rasterizationState.depthBiasEnable = VK_FALSE;
			rasterizationState.lineWidth = 1.0f;

			switch (InRasterizerState)
			{
			case ERasterizerState::NoCull:
				rasterizationState.cullMode = VK_CULL_MODE_NONE;
				break;
			case ERasterizerState::BackFaceCull:
				rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
				break;
			case ERasterizerState::FrontFaceCull:
				rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
				break;
			case ERasterizerState::Wireframe:
				rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
				break;
			};

			// Color blend state describes how blend factors are calculated (if used)
			// We need one blend attachment state per color attachment (even if blending is not used)
			VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
			blendAttachmentState[0].colorWriteMask = 0xf;
			blendAttachmentState[0].blendEnable = VK_FALSE;
			VkPipelineColorBlendStateCreateInfo colorBlendState = {};
			colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendState.attachmentCount = 1;
			colorBlendState.pAttachments = blendAttachmentState;

			// Viewport state sets the number of viewports and scissor used in this pipeline
			// Note: This is actually overridden by the dynamic states (see below)
			VkPipelineViewportStateCreateInfo viewportState = {};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.scissorCount = 1;

			// Multi sampling state
			// This example does not make use of multi sampling (for anti-aliasing), the state must still be set and passed to the pipeline
			VkPipelineMultisampleStateCreateInfo multisampleState = {};
			multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampleState.pSampleMask = nullptr;

			// Enable dynamic states
			// Most states are baked into the pipeline, but there are still a few dynamic states that can be changed within a command buffer
			// To be able to change these we need do specify which dynamic states will be changed using this pipeline. Their actual states are set later on in the command buffer.
			// For this example we will set the viewport and scissor using dynamic states
			std::vector<VkDynamicState> dynamicStateEnables;
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
			VkPipelineDynamicStateCreateInfo dynamicState = {};
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.pDynamicStates = dynamicStateEnables.data();
			dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

			// Depth and stencil state containing depth and stencil compare and test operations
			// We only use depth tests and want depth tests and writes to be enabled and compare with less or equal
			VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
			depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilState.depthTestEnable = VK_TRUE;
			depthStencilState.depthWriteEnable = VK_TRUE;
			depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			depthStencilState.depthBoundsTestEnable = VK_FALSE;
			depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
			depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
			depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
			depthStencilState.stencilTestEnable = VK_FALSE;
			depthStencilState.front = depthStencilState.back;

			switch (InDepthState)
			{
			case EDepthState::Disabled:
				depthStencilState.depthTestEnable = VK_FALSE;
				depthStencilState.depthWriteEnable = VK_FALSE;
				break;
			case EDepthState::Enabled:
				depthStencilState.depthTestEnable = VK_TRUE;
				depthStencilState.depthWriteEnable = VK_TRUE;
				break;
			}

			// Shaders
			std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

			{
				VkPipelineShaderStageCreateInfo curShader;
				// Vertex shader
				curShader.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				// Set pipeline stage for this shader
				curShader.stage = VK_SHADER_STAGE_VERTEX_BIT;
				// Load binary SPIR-V shader
				curShader.module = InVS->GetAs<VulkanShader>().GetModule();
				// Main entry point for the shader
				curShader.pName = InVS->GetAs<VulkanShader>().GetEntryPoint().c_str();
				SE_ASSERT(curShader.module != VK_NULL_HANDLE);

				shaderStages.push_back(curShader);
			}
			{
				VkPipelineShaderStageCreateInfo curShader;
				// Vertex shader
				curShader.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				// Set pipeline stage for this shader
				curShader.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				// Load binary SPIR-V shader
				curShader.module = InPS->GetAs<VulkanShader>().GetModule();
				// Main entry point for the shader
				curShader.pName = InPS->GetAs<VulkanShader>().GetEntryPoint().c_str();
				SE_ASSERT(curShader.module != VK_NULL_HANDLE);

				shaderStages.push_back(curShader);
			}

			// Set pipeline shader stage info
			pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			pipelineCreateInfo.pStages = shaderStages.data();

			// Assign the pipeline states to the pipeline creation info structure
			pipelineCreateInfo.pVertexInputState = &inputLayout.GetVertexInputState();
			pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
			pipelineCreateInfo.pRasterizationState = &rasterizationState;
			pipelineCreateInfo.pColorBlendState = &colorBlendState;
			pipelineCreateInfo.pMultisampleState = &multisampleState;
			pipelineCreateInfo.pViewportState = &viewportState;
			pipelineCreateInfo.pDepthStencilState = &depthStencilState;
			pipelineCreateInfo.renderPass = renderPass;
			pipelineCreateInfo.pDynamicState = &dynamicState;

			// Pipeline cache object
			VkPipelineCache pipelineCache;

			VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
			pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

			// Create rendering pipeline using the specified states
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &_pipeline));

			vkDestroyPipeline(device, _pipeline, nullptr);
			vkDestroyDescriptorSetLayout(device, _descriptorSetLayout, nullptr);
			vkDestroyPipelineLayout(device, _pipelineLayout, nullptr);

			vkDestroyPipelineCache(device, pipelineCache, nullptr);
			// Shader modules are no longer needed once the graphics pipeline has been created
			//vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
			//vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
		}
	};

	static VulkanGraphicInterface staticDGI;
}