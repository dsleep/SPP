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

	public:


		virtual void Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow)
		{
			DeviceInitialize();
			
			//initSwapchain();
			width = InitialWidth;
			height = InitialHeight;
			//setupSwapChain();
			//createCommandBuffers();

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

		virtual void BeginFrame()
		{

		}
		virtual void EndFrame()
		{

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
		virtual GPUReferencer< GPUTexture > CreateTexture(int32_t Width, int32_t Height, TextureFormat Format,
			std::shared_ptr< ArrayResource > RawData = nullptr,
			std::shared_ptr< ImageMeta > InMetaInfo = nullptr) override
		{
			return nullptr;
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