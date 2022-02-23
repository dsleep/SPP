// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "vulkan/vulkan.h"

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

	class VulkanGraphicsDevice : public GraphicsDevice
	{
	private:

		uint32_t width = 1280;
		uint32_t height = 720;

		// Vulkan instance, stores all per-application states
		VkInstance instance;
		std::vector<std::string> supportedInstanceExtensions;
		// Physical device (GPU) that Vulkan will use
		VkPhysicalDevice physicalDevice;
		// Stores physical device properties (for e.g. checking device limits)
		VkPhysicalDeviceProperties deviceProperties;
		// Stores the features available on the selected physical device (for e.g. checking if a feature is available)
		VkPhysicalDeviceFeatures deviceFeatures;
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
		VkDevice device;
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
			appInfo.apiVersion = 1;

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
						std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
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
					std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
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
				vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(err), err);
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
				vks::tools::exitFatal("No device with Vulkan support found", -1);
				return false;
			}
			// Enumerate devices
			std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
			err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());
			if (err) {
				vks::tools::exitFatal("Could not enumerate physical devices : \n" + vks::tools::errorString(err), err);
				return false;
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
				std::cout << "Available Vulkan devices" << "\n";
				for (uint32_t i = 0; i < gpuCount; i++) {
					VkPhysicalDeviceProperties deviceProperties;
					vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
					std::cout << "Device [" << i << "] : " << deviceProperties.deviceName << std::endl;
					std::cout << " Type: " << vks::tools::physicalDeviceTypeString(deviceProperties.deviceType) << "\n";
					std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << "\n";
				}
			//}
#endif

			physicalDevice = physicalDevices[selectedDevice];

			// Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
			vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
			vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

			// Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
			//getEnabledFeatures();

			// Vulkan device creation
			// This is handled by a separate class that gets a logical device representation
			// and encapsulates functions related to a device
			vulkanDevice = new vks::VulkanDevice(physicalDevice);
			VkResult res = vulkanDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain);
			if (res != VK_SUCCESS) {
				vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(res), res);
				return false;
			}
			device = vulkanDevice->logicalDevice;

			GGlobalVulkanDevice = device;

			// Get a graphics queue from the device
			vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

			// Find a suitable depth format
			VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
			assert(validDepthFormat);

			swapChain.connect(instance, physicalDevice, device);

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

		virtual void Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow)
		{
			DeviceInitialize();			
			initSwapchain();
			width = InitialWidth;
			height = InitialHeight;

			cmdPool = vulkanDevice->createCommandPool(swapChain.queueNodeIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
			setupSwapChain();
			createCommandBuffers();

			setupDepthStencil();
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

		void CreatePipelineState(EBlendState InBlendState,
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
			//// Vulkan uses the concept of rendering pipelines to encapsulate fixed states, replacing OpenGL's complex state machine
			//// A pipeline is then stored and hashed on the GPU making pipeline changes very fast
			//// Note: There are still a few dynamic states that are not directly part of the pipeline (but the info that they are used is)

			//VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
			//pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			//// The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
			//pipelineCreateInfo.layout = pipelineLayout;
			//// Renderpass this pipeline is attached to
			//pipelineCreateInfo.renderPass = renderPass;

			//// Construct the different states making up the pipeline

			//// Input assembly state describes how primitives are assembled
			//// This pipeline will assemble vertex data as a triangle lists (though we only use one triangle)
			//VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
			//inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

			//switch (InTopology)
			//{
			//case EDrawingTopology::TriangleList:
			//	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			//	break;
			//case EDrawingTopology::TriangleStrip:
			//	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			//	break;
			//case EDrawingTopology::LineList:
			//	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			//	break;
			//case EDrawingTopology::PointList:
			//	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			//	break;
			//default:
			//	SE_ASSERT(false);
			//}


			//// Rasterization state
			//VkPipelineRasterizationStateCreateInfo rasterizationState = {};
			//rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			//rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
			//rasterizationState.cullMode = VK_CULL_MODE_NONE;
			//rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			//rasterizationState.depthClampEnable = VK_FALSE;
			//rasterizationState.rasterizerDiscardEnable = VK_FALSE;
			//rasterizationState.depthBiasEnable = VK_FALSE;
			//rasterizationState.lineWidth = 1.0f;

			//// Color blend state describes how blend factors are calculated (if used)
			//// We need one blend attachment state per color attachment (even if blending is not used)
			//VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
			//blendAttachmentState[0].colorWriteMask = 0xf;
			//blendAttachmentState[0].blendEnable = VK_FALSE;
			//VkPipelineColorBlendStateCreateInfo colorBlendState = {};
			//colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			//colorBlendState.attachmentCount = 1;
			//colorBlendState.pAttachments = blendAttachmentState;

			//// Viewport state sets the number of viewports and scissor used in this pipeline
			//// Note: This is actually overridden by the dynamic states (see below)
			//VkPipelineViewportStateCreateInfo viewportState = {};
			//viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			//viewportState.viewportCount = 1;
			//viewportState.scissorCount = 1;

			//// Enable dynamic states
			//// Most states are baked into the pipeline, but there are still a few dynamic states that can be changed within a command buffer
			//// To be able to change these we need do specify which dynamic states will be changed using this pipeline. Their actual states are set later on in the command buffer.
			//// For this example we will set the viewport and scissor using dynamic states
			//std::vector<VkDynamicState> dynamicStateEnables;
			//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
			//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
			//VkPipelineDynamicStateCreateInfo dynamicState = {};
			//dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			//dynamicState.pDynamicStates = dynamicStateEnables.data();
			//dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

			//// Depth and stencil state containing depth and stencil compare and test operations
			//// We only use depth tests and want depth tests and writes to be enabled and compare with less or equal
			//VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
			//depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			//depthStencilState.depthTestEnable = VK_TRUE;
			//depthStencilState.depthWriteEnable = VK_TRUE;
			//depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			//depthStencilState.depthBoundsTestEnable = VK_FALSE;
			//depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
			//depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
			//depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
			//depthStencilState.stencilTestEnable = VK_FALSE;
			//depthStencilState.front = depthStencilState.back;

			//// Multi sampling state
			//// This example does not make use of multi sampling (for anti-aliasing), the state must still be set and passed to the pipeline
			//VkPipelineMultisampleStateCreateInfo multisampleState = {};
			//multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			//multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			//multisampleState.pSampleMask = nullptr;

			//// Vertex input descriptions
			//// Specifies the vertex input parameters for a pipeline

			//// Vertex input binding
			//// This example uses a single vertex input binding at binding point 0 (see vkCmdBindVertexBuffers)
			//VkVertexInputBindingDescription vertexInputBinding = {};
			//vertexInputBinding.binding = 0;
			//vertexInputBinding.stride = sizeof(Vertex);
			//vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			//// Input attribute bindings describe shader attribute locations and memory layouts
			//std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributs;

			////switch (InType)
			////{
			////case InputLayoutElementType::Float3:
			////	return DXGI_FORMAT_R32G32B32_FLOAT;
			////	break;
			////case InputLayoutElementType::Float2:
			////	return DXGI_FORMAT_R32G32_FLOAT;
			////	break;
			////case InputLayoutElementType::UInt:
			////	return DXGI_FORMAT_R32_UINT;
			////	break;
			////}
			////return DXGI_FORMAT_UNKNOWN;

			//// These match the following shader layout (see triangle.vert):
			////	layout (location = 0) in vec3 inPos;
			////	layout (location = 1) in vec3 inColor;
			//// Attribute location 0: Position
			//vertexInputAttributs[0].binding = 0;
			//vertexInputAttributs[0].location = 0;
			//// Position attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
			//vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			//vertexInputAttributs[0].offset = offsetof(Vertex, position);
			//// Attribute location 1: Color
			//vertexInputAttributs[1].binding = 0;
			//vertexInputAttributs[1].location = 1;
			//// Color attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
			//vertexInputAttributs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			//vertexInputAttributs[1].offset = offsetof(Vertex, color);

			//// Vertex input state used for pipeline creation
			//VkPipelineVertexInputStateCreateInfo vertexInputState = {};
			//vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			//vertexInputState.vertexBindingDescriptionCount = 1;
			//vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
			//vertexInputState.vertexAttributeDescriptionCount = 2;
			//vertexInputState.pVertexAttributeDescriptions = vertexInputAttributs.data();

			//// Shaders
			//std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

			//// Vertex shader
			//shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			//// Set pipeline stage for this shader
			//shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			//// Load binary SPIR-V shader
			//shaderStages[0].module = InVS->GetAs<VulkanShader>().GetModule();
			//// Main entry point for the shader
			//shaderStages[0].pName = "main";
			//SE_ASSERT(shaderStages[0].module != VK_NULL_HANDLE);

			//// Fragment shader
			//shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			//// Set pipeline stage for this shader
			//shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			//// Load binary SPIR-V shader
			//shaderStages[1].module = InPS->GetAs<VulkanShader>().GetModule();
			//// Main entry point for the shader
			//shaderStages[1].pName = "main";
			//SE_ASSERT(shaderStages[1].module != VK_NULL_HANDLE);

			//// Set pipeline shader stage info
			//pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			//pipelineCreateInfo.pStages = shaderStages.data();

			//// Assign the pipeline states to the pipeline creation info structure
			//pipelineCreateInfo.pVertexInputState = &vertexInputState;
			//pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
			//pipelineCreateInfo.pRasterizationState = &rasterizationState;
			//pipelineCreateInfo.pColorBlendState = &colorBlendState;
			//pipelineCreateInfo.pMultisampleState = &multisampleState;
			//pipelineCreateInfo.pViewportState = &viewportState;
			//pipelineCreateInfo.pDepthStencilState = &depthStencilState;
			//pipelineCreateInfo.renderPass = renderPass;
			//pipelineCreateInfo.pDynamicState = &dynamicState;

			//// Create rendering pipeline using the specified states
			//VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

			//// Shader modules are no longer needed once the graphics pipeline has been created
			//vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
			//vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
		}


		virtual void BeginFrame()
		{
			//// Get next image in the swap chain (back/front buffer)
			//VK_CHECK_RESULT(swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer));

			//// Use a fence to wait until the command buffer has finished execution before using it again
			//VK_CHECK_RESULT(vkWaitForFences(device, 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX));
			//VK_CHECK_RESULT(vkResetFences(device, 1, &waitFences[currentBuffer]));

			/////
			//VkCommandBufferBeginInfo cmdBufInfo = {};
			//cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			//cmdBufInfo.pNext = nullptr;

			//// Set clear values for all framebuffer attachments with loadOp set to clear
			//// We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
			//VkClearValue clearValues[2];
			//clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
			//clearValues[1].depthStencil = { 1.0f, 0 };

			//VkRenderPassBeginInfo renderPassBeginInfo = {};
			//renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			//renderPassBeginInfo.pNext = nullptr;
			//renderPassBeginInfo.renderPass = renderPass;
			//renderPassBeginInfo.renderArea.offset.x = 0;
			//renderPassBeginInfo.renderArea.offset.y = 0;
			//renderPassBeginInfo.renderArea.extent.width = width;
			//renderPassBeginInfo.renderArea.extent.height = height;
			//renderPassBeginInfo.clearValueCount = 2;
			//renderPassBeginInfo.pClearValues = clearValues;
			//// Set target frame buffer
			//renderPassBeginInfo.framebuffer = frameBuffers[currentBuffer];


			//auto& commandBuffer = drawCmdBuffers[currentBuffer];

			//VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

			//// Start the first sub pass specified in our default render pass setup by the base class
			//// This will clear the color and depth attachment
			//vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			//// Update dynamic viewport state
			//VkViewport viewport = {};
			//viewport.height = (float)height;
			//viewport.width = (float)width;
			//viewport.minDepth = (float)0.0f;
			//viewport.maxDepth = (float)1.0f;
			//vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			//// Update dynamic scissor state
			//VkRect2D scissor = {};
			//scissor.extent.width = width;
			//scissor.extent.height = height;
			//scissor.offset.x = 0;
			//scissor.offset.y = 0;
			//vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

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

			// Present the current buffer to the swap chain
			// Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
			// This ensures that the image is not presented to the windowing system until all commands have been submitted
			VkResult present = swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete);
			if (!((present == VK_SUCCESS) || (present == VK_SUBOPTIMAL_KHR))) {
				VK_CHECK_RESULT(present);
			}
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

	static VulkanGraphicInterface staticDGI;
}