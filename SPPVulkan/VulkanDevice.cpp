// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#include <VulkanDevice.h>
#include "VulkanTexture.h"
#include "VulkanShaders.h"
#include "VulkanDebug.h"
#include "VulkanFrameBuffer.hpp"
#include "VulkanDebugDrawing.h"
#include "VulkanRenderScene.h"

#include <unordered_set>
#include "SPPLogging.h"
#include "SPPSceneRendering.h"
#include "SPPGraphicsO.h"
#include "SPPSDFO.h"

#include "vulkan/vulkan_win32.h"

//IMGUI
#include "imgui_impl_vulkan.h"

#define ALLOW_DEVICE_FEATURES2 0
#define ALLOW_IMGUI 0

namespace SPP
{
	extern LogEntry LOG_VULKAN;

	//TODO Get rid of this
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;


	void* vkAllocationFunction(
		void* pUserData,
		size_t                                      size,
		size_t                                      alignment,
		VkSystemAllocationScope                     allocationScope)
	{
		return nullptr;
	}

	void* vkReallocationFunction(
		void* pUserData,
		void* pOriginal,
		size_t                                      size,
		size_t                                      alignment,
		VkSystemAllocationScope                     allocationScope)
	{
		return nullptr;
	}

	void vkFreeFunction(
		void* pUserData,
		void* pMemory)
	{

	}

	void vkInternalAllocationNotification(
		void* pUserData,
		size_t                                      size,
		VkInternalAllocationType                    allocationType,
		VkSystemAllocationScope                     allocationScope)
	{

	}

	void vkInternalFreeNotification(
		void* pUserData,
		size_t                                      size,
		VkInternalAllocationType                    allocationType,
		VkSystemAllocationScope                     allocationScope)
	{

	}


	VkAllocationCallbacks GAllocations =
	{
		.pUserData = nullptr,
		.pfnAllocation = vkAllocationFunction,
		.pfnReallocation = vkReallocationFunction,
		.pfnFree = vkFreeFunction,
		.pfnInternalAllocation = vkInternalAllocationNotification,
		.pfnInternalFree = vkInternalFreeNotification
	};

	extern GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GraphicsDevice* InOwner, GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	
	VkPipelineVertexInputStateCreateInfo& VulkanInputLayout::GetVertexInputState()
	{
		return _vertexInputState;
	}

	void VulkanInputLayout::InitializeLayout(const std::vector<VertexStream>& vertexStreams)
	{
		for (auto& curStream : vertexStreams)
		{
			VkVertexInputBindingDescription vertexInputBinding = {};
			vertexInputBinding.binding = (uint32_t)_inputBindings.size();
			vertexInputBinding.stride = curStream.Size;
			vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			for (auto& curAttribute : curStream.Attributes)
			{
				VkVertexInputAttributeDescription Desc{};

				switch (curAttribute.Type)
				{
				case InputLayoutElementType::Float3:
					Desc.format = VK_FORMAT_R32G32B32_SFLOAT;
					break;
				case InputLayoutElementType::Float2:
					Desc.format = VK_FORMAT_R32G32_SFLOAT;
					break;
				case InputLayoutElementType::Float:
					Desc.format = VK_FORMAT_R32_SFLOAT;
					break;
				case InputLayoutElementType::UInt32:
					Desc.format = VK_FORMAT_R32_UINT;
					break;
				case InputLayoutElementType::UInt8_4:
					Desc.format = VK_FORMAT_R8G8B8A8_UNORM;
					break;
				default:
					SE_ASSERT(false);
					break;
				}

				Desc.binding = (uint32_t)_inputBindings.size();
				Desc.location = (uint32_t)_inputAttributes.size();
				Desc.offset = (uint32_t)(curAttribute.Offset);

				// todo real proper check
				SE_ASSERT(Desc.offset < 128);

				_inputAttributes.push_back(Desc);
			}

			_inputBindings.push_back(vertexInputBinding);
		}

		_vertexInputState = {};
		_vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		_vertexInputState.vertexBindingDescriptionCount = (uint32_t)_inputBindings.size();
		_vertexInputState.pVertexBindingDescriptions = _inputBindings.size() ? _inputBindings.data() : nullptr;
		_vertexInputState.vertexAttributeDescriptionCount = (uint32_t)_inputAttributes.size();
		_vertexInputState.pVertexAttributeDescriptions = _inputAttributes.size() ? _inputAttributes.data() : nullptr;
	}

	//////////////////////////


	VkResult VulkanGraphicsDevice::createInstance(bool enableValidation)
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
			SPP_LOG(LOG_VULKAN, LOG_INFO, "Found %d Extensions", extCount);

			std::vector<VkExtensionProperties> extensions(extCount);
			if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
			{
				for (VkExtensionProperties extension : extensions)
				{
					SPP_LOG(LOG_VULKAN, LOG_INFO, " - Extension %s", extension.extensionName);
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

	bool VulkanGraphicsDevice::DeviceInitialize()
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

#if ALLOW_DEVICE_FEATURES2
		vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

		if (indexingFeatures.descriptorBindingPartiallyBound && indexingFeatures.runtimeDescriptorArray)
		{
			// all set to use unbound arrays of textures
			SPP_LOG(LOG_VULKAN, LOG_INFO, "BOUNDLESS SUPPORT!");
		}
#endif

		// [POI] Enable required extensions
		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

		enabledFeatures.shaderFloat64 = true;

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
		GGlobalVulkanGI = this;

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

		{
			auto textureData = std::make_shared< ArrayResource >();
			auto textureAccess = textureData->InitializeFromType<Color4>(128 * 128);

			for (uint32_t Iter = 0; Iter < textureAccess.GetCount(); Iter++)
			{
				textureAccess[Iter][1] = 255;
				textureAccess[Iter][3] = 255;
			}
			_defaultTexture = Make_GPU(VulkanTexture, this, 128, 128, TextureFormat::RGBA_8888, textureData, nullptr);
		}

		return true;
	}

	void VulkanGraphicsDevice::nextFrame() {}
	void VulkanGraphicsDevice::updateOverlay() {}

	void VulkanGraphicsDevice::createStaticDrawInfo()
	{
		//move to nicer spot
		_defaultMaterial = CreateMaterial();

		_meshvertexShader = CreateShader();
		_meshpixelShader = CreateShader();
	
		_defaultMaterial->SetMaterialArgs({ .vertexShader = _meshvertexShader, .pixelShader = _meshpixelShader });

		_meshvertexShader->Initialize(EShaderType::Vertex);
		_meshvertexShader->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_vs");
		_meshpixelShader->Initialize(EShaderType::Pixel);
		_meshpixelShader->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_ps");

		//create buffer and simple descriptor set of camera and 

		//std::shared_ptr< ArrayResource > _staticInstanceDrawInfoCPU;
		//GPUReferencer< class VulkanBuffer > _staticInstanceDrawInfoGPU;

		_staticInstanceDrawInfoCPU = std::make_shared< ArrayResource >();
		_staticInstanceDrawInfoSpan = _staticInstanceDrawInfoCPU->InitializeFromType< StaticDrawParams >(250000);
		_staticInstanceDrawInfoGPU = Vulkan_CreateStaticBuffer(this, GPUBufferType::Simple, _staticInstanceDrawInfoCPU);

		_staticInstanceDrawLeaseManager = std::make_unique <  FrameTrackedLeaseManager< TSpan< StaticDrawParams > > >(_staticInstanceDrawInfoSpan);
	}

	//void VulkanGraphicsDevice::createPBRLayout()
	//{
	//	// update these values to be useful for your specific use case
	//	VkDescriptorSetLayoutBinding layoutBinding{};
	//	layoutBinding.binding = 0u;
	//	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	//	layoutBinding.descriptorCount = 5000u;
	//	layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	//	VkDescriptorBindingFlagsEXT bindFlag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;

	//	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extendedInfo{};
	//	extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	//	extendedInfo.pNext = nullptr;
	//	extendedInfo.bindingCount = 1u;
	//	extendedInfo.pBindingFlags = &bindFlag;

	//	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	//	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	//	layoutInfo.pNext = &extendedInfo;
	//	layoutInfo.flags = 0;
	//	layoutInfo.bindingCount = 1u;
	//	layoutInfo.pBindings = &layoutBinding;

	//	VkDescriptorSetLayout PBR_SetLayouts;
	//	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &PBR_SetLayouts));
	//}

	void VulkanGraphicsDevice::createPipelineCache()
	{
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
	}

	void VulkanGraphicsDevice::createCommandPool()
	{
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
	}

	void VulkanGraphicsDevice::createSynchronizationPrimitives()
	{
		// Wait fences to sync command buffer access
		VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

		for (int32_t Iter = 0; Iter < swapChain.imageCount; Iter++)
		{
			_waitFences[Iter] = Make_GPU(SafeVkFence, this, fenceCreateInfo);
		}
	}

	void VulkanGraphicsDevice::initSwapchain()
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

	void VulkanGraphicsDevice::setupSwapChain()
	{
		swapChain.create(&width, &height, settings.vsync);

		_colorTarget = std::make_unique< VulkanFramebuffer >(this, vulkanDevice);
		_colorTarget->addAttachment(
			{
				.width = width,
				.height = height,
				.layerCount = 1,
				.format = VK_FORMAT_R8G8B8A8_UNORM,
				.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
			}
		);
		_colorTarget->addAttachment(
			{
				.width = width,
				.height = height,
				.layerCount = 1,
				.format = VK_FORMAT_D32_SFLOAT_S8_UINT,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
			}
		);		
		VK_CHECK_RESULT(_colorTarget->createRenderPass());
		_colorTarget->createSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

		/*
		_deferredMaterialMRTs = std::make_unique< VulkanFramebuffer >(vulkanDevice);

		_deferredMaterialMRTs->addAttachment(
			{
				.width = width,
				.height = height,
				.layerCount = 1,
				.format = VK_FORMAT_D32_SFLOAT_S8_UINT,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
			}
		);

		_deferredMaterialMRTs->addAttachment(
			{
				.width = width,
				.height = height,
				.layerCount = 1,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
			}
		);

		_deferredMaterialMRTs->addAttachment(
			{
				.width = width,
				.height = height,
				.layerCount = 1,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
			}
		);
		
		_deferredMaterialMRTs->addAttachment(
			{
				.width = width,
				.height = height,
				.layerCount = 1,
				.format = VK_FORMAT_R8G8B8A8_UINT,
				.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
			}
		);

		VK_CHECK_RESULT(_deferredMaterialMRTs->createRenderPass());
		_deferredMaterialMRTs->createSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		*/
	}

	void VulkanGraphicsDevice::createCommandBuffers()
	{
		VkCommandBufferAllocateInfo cmdBufAllocateInfo =
			vks::initializers::commandBufferAllocateInfo(
				cmdPool,
				VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				1);

		VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

		// Create one command buffer for each swap chain image and reuse for rendering
		for (int32_t Iter = 0; Iter < swapChain.imageCount; Iter++)
		{
			_drawCmdBuffers[Iter] = Make_GPU(SafeVkCommandBuffer, this, cmdBufAllocateInfo);
			_copyCmdBuffers[Iter].cmdBuf = Make_GPU(SafeVkCommandBuffer, this, cmdBufAllocateInfo);
			_copyCmdBuffers[Iter].fence = Make_GPU(SafeVkFence, this, fenceInfo);
		}
	}

	void VulkanGraphicsDevice::destroyCommandBuffers()
	{
		for (auto& curBuf : _drawCmdBuffers)
		{
			curBuf.Reset();
		}
		for (auto& curCopy : _copyCmdBuffers)
		{
			curCopy.cmdBuf.Reset();
			curCopy.fence.Reset();
			curCopy.bHasBegun = false;
		}
	}

	void VulkanGraphicsDevice::setupRenderPass()
	{
		std::array<VkAttachmentDescription, 1> attachments = {};
		// Color attachment
		attachments[0].format = swapChain.colorFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.pDepthStencilAttachment = nullptr;
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


	void VulkanGraphicsDevice::setupFrameBuffer()
	{
		VkImageView attachments;

		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = NULL;
		frameBufferCreateInfo.renderPass = renderPass;
		frameBufferCreateInfo.attachmentCount = 1;
		frameBufferCreateInfo.pAttachments = &attachments;
		frameBufferCreateInfo.width = width;
		frameBufferCreateInfo.height = height;
		frameBufferCreateInfo.layers = 1;

		// Create frame buffers for every swap chain image
		for (uint32_t i = 0; i < swapChain.imageCount; i++)
		{
			attachments = swapChain.buffers[i].view;
			_frameBuffers[i] = Make_GPU(SafeVkFrameBuffer, this, frameBufferCreateInfo);
		}
	}

	void VulkanGraphicsDevice::destroyFrameBuffer()
	{
		for (auto& curBuffer : _frameBuffers)
		{
			curBuffer.Reset();
		}
	}

	VkDevice VulkanGraphicsDevice::GetDevice()
	{
		return device;
	}

	VkRenderPass VulkanGraphicsDevice::GetBaseRenderPass()
	{
		return renderPass;
	}

	void VulkanGraphicsDevice::Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow)
	{
		GPUThreadIDOverride tempOverride;

#if PLATFORM_WINDOWS
		window = (HWND)OSWindow;
		windowInstance = GetModuleHandle(NULL);
#endif

		width = InitialWidth;
		height = InitialHeight;

		DeviceInitialize();
		initSwapchain();

		cmdPool = vulkanDevice->createCommandPool(swapChain.queueNodeIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		setupSwapChain();
		createCommandBuffers();
		createSynchronizationPrimitives();

		setupRenderPass();
		setupFrameBuffer();
		CreateDescriptorPool();

		
#if ALLOW_IMGUI
		//IMGUI
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(width, height);
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		ImGui_ImplVulkan_InitInfo initInfo = { 0 };

		initInfo.Instance = instance;
		initInfo.PhysicalDevice = physicalDevice;
		initInfo.Device = device;

		initInfo.QueueFamily = swapChain.queueNodeIndex;
		initInfo.Queue = queue;
		initInfo.PipelineCache = pipelineCache;
		initInfo.DescriptorPool = _globalPool;

		initInfo.Subpass = 0;
		initInfo.MinImageCount = 2;          // >= 2
		initInfo.ImageCount = swapChain.imageCount;             // >= MinImageCount
		initInfo.MSAASamples = (VkSampleCountFlagBits)0;            // >= VK_SAMPLE_COUNT_1_BIT (0 -> default to VK_SAMPLE_COUNT_1_BIT)
		//initInfo.Allocator;// const VkAllocationCallbacks* Allocator;
		//initInfo.CheckVkResultFn;// void                            (*CheckVkResultFn)(VkResult err);
#endif
		//IMGUI
		{

#if ALLOW_IMGUI
			ImGui_ImplVulkan_Init(&initInfo, renderPass);

			auto& cmdBuffer = GetCopyCommandBuffer();
			ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);

			//ImGui_ImplVulkan_DestroyFontUploadObjects
			//ImGui_ImplVulkan_Shutdown()
#endif

			auto CurResource = InternalLinkedList<GlobalGraphicsResource>::GetRoot();
			while (CurResource)
			{
				CurResource->Initialize(this);
				CurResource = CurResource->GetNext();
			};
		}
	}

	void VulkanGraphicsDevice::Shutdown()
	{
		auto CurResource = InternalLinkedList<GlobalGraphicsResource>::GetRoot();
		while (CurResource)
		{
			CurResource->Shutdown(this);
			CurResource = CurResource->GetNext();
		};

		for (auto& fence : _waitFences)
		{
			fence.Reset();
		}

		destroyFrameBuffer();
		destroyCommandBuffers();

		_defaultTexture.Reset();
		_colorTarget.reset();
		_piplineStateMap.clear();

		Flush();
	}

	void VulkanGraphicsDevice::ResizeBuffers(int32_t NewWidth, int32_t NewHeight)
	{
		if (width != NewWidth || height != NewHeight)
		{
			// Ensure all operations on the device have been finished before destroying resources
			vkDeviceWaitIdle(device);

			// Recreate swap chain
			width = NewWidth;
			height = NewHeight;
			setupSwapChain();

			destroyFrameBuffer();
			setupFrameBuffer();

			destroyCommandBuffers();
			createCommandBuffers();

			vkDeviceWaitIdle(device);
		}
	}

	void VulkanGraphicsDevice::DyingResource(class GPUResource* InResourceToKill)
	{
		if (IsOnCPUThread())
		{
			_cpuPushedDyingResources.push_back(InResourceToKill);
		}
		else if (IsOnGPUThread())
		{
			_gpuPushedDyingResources.push_back(InResourceToKill);
		}
		else
		{
			// uh ohs can't be on neither
			SE_ASSERT(false);
		}
	}

	int32_t VulkanGraphicsDevice::GetDeviceWidth() const
	{
		return width;
	}

	int32_t VulkanGraphicsDevice::GetDeviceHeight() const
	{
		return height;
	}

#define MAX_MESH_ELEMENTS 1024
#define MAX_TEXTURE_COUNT 2048
#define DYNAMIC_MAX_COUNT 20 * 1024	

	void VulkanGraphicsDevice::CreateDescriptorPool()
	{
		SE_ASSERT(swapChain.imageCount);

		{
			std::vector<VkDescriptorPoolSize> simplePool = {
				vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000),
				vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000),
				
				vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000),
				vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1000),
				vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000),
			};
			auto poolCreateInfo = vks::initializers::descriptorPoolCreateInfo(simplePool, 1000);

			for (int32_t Iter = 0; Iter < swapChain.imageCount; Iter++)
			{
				VkDescriptorPool curPool;
				VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &curPool));
				_perDrawPools.push_back(curPool);
			}
		}

		//IMGUI primarily
		{
			std::vector < VkDescriptorPoolSize > globalPool =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			};

			auto poolCreateInfo = vks::initializers::descriptorPoolCreateInfo(globalPool, 1000);
			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &_globalPool));
		}
	}

	void VulkanGraphicsDevice::CreateInputLayout(GPUReferencer < GPUInputLayout > InLayout)
	{

	}


	VkCommandBuffer& VulkanGraphicsDevice::GetCopyCommandBuffer()
	{
		SE_ASSERT(IsOnGPUThread());
		SE_ASSERT(!bDrawPhase);
			
		auto& curCopier = _copyCmdBuffers[currentBuffer];

		if (!curCopier.bHasBegun)
		{
			VK_CHECK_RESULT(vkWaitForFences(device, 1, &curCopier.fence->Get(), VK_TRUE, UINT64_MAX));
			VK_CHECK_RESULT(vkResetFences(device, 1, &curCopier.fence->Get()));

			VK_CHECK_RESULT(vkResetCommandBuffer(curCopier.cmdBuf->Get(), 0));
			VkCommandBufferBeginInfo cmdBufInfo = {};
			cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cmdBufInfo.pNext = nullptr;
			cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			VK_CHECK_RESULT(vkBeginCommandBuffer(curCopier.cmdBuf->Get(), &cmdBufInfo));

			curCopier.bHasBegun = true;
		}

		return curCopier.cmdBuf->Get();
	}

	void VulkanGraphicsDevice::SubmitCopyCommands()
	{
		auto& curCopier = _copyCmdBuffers[currentBuffer];
		
		if (curCopier.bHasBegun)
		{
			VK_CHECK_RESULT(vkEndCommandBuffer(curCopier.cmdBuf->Get()));

			VkSubmitInfo submitInfo = vks::initializers::submitInfo();
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &curCopier.cmdBuf->Get();
			// Submit to the queue
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, curCopier.fence->Get()));

			curCopier.bHasBegun = false;
		}
	}

	void VulkanGraphicsDevice::Flush()
	{
		GPUThreadIDOverride tempOverride;

		vkDeviceWaitIdle(device);

		static_assert(MAX_IN_FLIGHT == 3);
		std::array< std::vector<GPUResource*>*, MAX_IN_FLIGHT + 2 > flushDying =
		{
			&_cpuPushedDyingResources,
			&_gpuPushedDyingResources,
			&_dyingResources[0],
			&_dyingResources[1],
			&_dyingResources[2]
		};

		for( int32_t Iter = 0; Iter < flushDying.size(); Iter++)
		{
			for (auto& curResource : *flushDying[Iter])
			{
				delete curResource;
			}
			flushDying[Iter]->clear();
		}
	}

	void VulkanGraphicsDevice::BeginFrame()
	{
		SubmitCopyCommands();

		// Acquire the next image from the swap chain
		VkResult result = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);
		// Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
		if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
		{
			//windowResize();
		}
		else
		{
			VK_CHECK_RESULT(result);
		}

		STDElapsedTimer priorFrame;

		// Use a fence to wait until the command buffer has finished execution before using it again
		VK_CHECK_RESULT(vkWaitForFences(device, 1, &_waitFences[currentBuffer]->Get(), VK_TRUE, UINT64_MAX));

		auto currentElapsedMS = priorFrame.getElapsedMilliseconds();
		if (currentElapsedMS > 5)
		{
			SPP_LOG(LOG_VULKAN, LOG_INFO, "LONG FRAME");
		}

		VK_CHECK_RESULT(vkResetFences(device, 1, &_waitFences[currentBuffer]->Get()));

		if (!_dyingResources[currentBuffer].empty())
		{
			for (auto& curResource : _dyingResources[currentBuffer])
			{
				delete curResource;
			}
			_dyingResources[currentBuffer].clear();
		}
		if (!_gpuPushedDyingResources.empty())
		{
			_dyingResources[currentBuffer].insert(_dyingResources[currentBuffer].end(), _gpuPushedDyingResources.begin(), _gpuPushedDyingResources.end());
			_gpuPushedDyingResources.clear();
		}
		
		_perFrameScratchBuffer.FrameCompleted(currentBuffer);
		_staticInstanceDrawLeaseManager->ClearTag(currentBuffer);

		VK_CHECK_RESULT(vkResetDescriptorPool(device, _perDrawPools[currentBuffer], 0));

		auto& commandBuffer = _drawCmdBuffers[currentBuffer]->Get();
		VK_CHECK_RESULT(vkResetCommandBuffer(commandBuffer, 0));
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufInfo.pNext = nullptr;
		cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;		
		VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

		GraphicsDevice::BeginFrame();

#if ALLOW_IMGUI
		//IMGUI
		ImGui::NewFrame();
		ImGui_ImplVulkan_NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(width, height));

		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("Window", nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration);
#endif

		bDrawPhase = true;
	}

	void VulkanGraphicsDevice::SyncGPUData()
	{
		GraphicsDevice::SyncGPUData();
		if (!_cpuPushedDyingResources.empty())
		{
			_dyingResources[currentBuffer].insert(_dyingResources[currentBuffer].end(), _cpuPushedDyingResources.begin(), _cpuPushedDyingResources.end());
			_cpuPushedDyingResources.clear();
		}
	}

	//IMGUI
	void VulkanGraphicsDevice::DrawDebugText(const Vector2i& InPosition, const char* Text, const Color3& InColor)
	{
#if ALLOW_IMGUI
		ImGui::SetCursorPosX(InPosition[0]);
		ImGui::SetCursorPosY(InPosition[1]);
		ImGui::Text(Text);
#endif
	}

	void VulkanGraphicsDevice::EndFrame()
	{
		bDrawPhase = false;

		//IMGUI
		//ImGui::ShowDemoWindow();
		auto& commandBuffer = _drawCmdBuffers[currentBuffer]->Get();

		//ImGui::SetCursorPosX(0);

		//ImGui::SetCursorPosY(0);
		//char inputText[200] = { 0 };
		//inputText[0] = '>';
		//inputText[1] = 0;
		//float tHeight = ImGui::CalcTextSize(">AB").y;

		//ImGui::InputText("##text1", inputText, sizeof(inputText));

		auto currentElapsedMS = _timer.getElapsedMilliseconds();
		if (_FPS.empty())
		{
			_FPS.resize(30);
		}
		_FPS[_currentFPSIdx] = currentElapsedMS;
#if ALLOW_IMGUI

		ImGui::SetCursorPosX(width - 325);
		ImGui::SetCursorPosY(25);

		ImGui::PlotHistogram("", _FPS.data(), _FPS.size(), _currentFPSIdx, "0-33(ms)", 0.0f, 33.333f, ImVec2(300, 100));

#if 1
		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = GetBackBufferRenderPass();
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 0;	
		renderPassBeginInfo.pClearValues = nullptr;
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = GetActiveFrameBuffer();

		// Start the first sub pass specified in our default render pass setup by the base class
		// This will clear the color and depth attachment
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.width = (float)width;
		viewport.height = (float)height;
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

		ImGui::End();
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

		vkCmdEndRenderPass(commandBuffer);
#else
		ImGui::End();
		ImGui::Render();
#endif

#endif
		_currentFPSIdx = (_currentFPSIdx+1) % _FPS.size();

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
		submitInfo.pCommandBuffers = &_drawCmdBuffers[currentBuffer]->Get(); // Command buffers(s) to execute in this batch (submission)
		submitInfo.commandBufferCount = 1;                           // One command buffer

		// Submit to the graphics queue passing a wait fence
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, _waitFences[currentBuffer]->Get()));

		VkResult result = swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete);
		if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) 
		{
			if (result == VK_ERROR_OUT_OF_DATE_KHR) 
			{
				// Swap chain is no longer compatible with the surface and needs to be recreated
				//windowResize();
				return;
			}
			else 
			{
				VK_CHECK_RESULT(result);
			}
		}
		//VK_CHECK_RESULT(vkQueueWaitIdle(queue));

		GraphicsDevice::EndFrame();
	}

	void VulkanGraphicsDevice::MoveToNextFrame()
	{
	}

	VulkanPipelineState::VulkanPipelineState(GraphicsDevice* InOwner) : PipelineState(InOwner)
	{

	}

	VulkanPipelineState::~VulkanPipelineState()
	{

		auto device = GGlobalVulkanDevice;

		if (_pipeline)
		{
			vkDestroyPipeline(device, _pipeline, nullptr);
			_pipeline = nullptr;
		}
		if (_pipelineLayout)
		{
			vkDestroyPipelineLayout(device, _pipelineLayout, nullptr);
			_pipelineLayout = nullptr;
		}
		if (_pipelineLayout)
		{
			vkDestroyPipelineLayout(device, _pipelineLayout, nullptr);
			_pipelineLayout = nullptr;
		}
		
		for (auto& curSetLayout : _descriptorSetLayouts)
		{
			vkDestroyDescriptorSetLayout(device, curSetLayout, nullptr);
		}
		_descriptorSetLayouts.clear();
		

		//vkDestroyPipelineLayout(device, _pipelineLayout, nullptr);
		//vkDestroyPipelineCache(device, pipelineCache, nullptr);
	}

	void MergeBindingSet(const std::vector<DescriptorSetLayoutData>& InDescriptorSet,
		std::map<uint8_t, std::vector<VkDescriptorSetLayoutBinding> > &oSetLayoutBindings)
	{
		for (auto& curSet : InDescriptorSet)
		{
			auto foundEle = oSetLayoutBindings.find(curSet.set_number);
			if (foundEle != oSetLayoutBindings.end())
			{
				std::vector<VkDescriptorSetLayoutBinding> &setLayoutBindings = foundEle->second;

				for (auto& newBinding : curSet.bindings)
				{
					bool bDoAdd = true;

					for (auto& curBinding : setLayoutBindings)
					{
						if (curBinding.binding == newBinding.binding)
						{
							curBinding.stageFlags |= newBinding.stageFlags;
							SE_ASSERT(curBinding.descriptorCount == curBinding.descriptorCount
								&& curBinding.descriptorType == curBinding.descriptorType);

							bDoAdd = false;
							break;
						}
					}

					if (bDoAdd)
					{
						setLayoutBindings.insert(setLayoutBindings.end(), newBinding);
					}
				}
			}
			else
			{
				oSetLayoutBindings[(uint8_t)curSet.set_number] = curSet.bindings;
			}
		}
	}

	void VulkanPipelineState::Initialize(EBlendState InBlendState,
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
		auto device = GGlobalVulkanDevice;

		auto renderPass = _renderPass ? _renderPass : GGlobalVulkanGI->GetColorFrameData().renderPass;

		//auto renderPass = GGlobalVulkanGI->GetBaseRenderPass();

		if (InVS && InPS)
		{
			auto& inputLayout = InLayout->GetAs<VulkanInputLayout>();

			// Shaders
			std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

			auto& vsSet = InVS->GetAs<VulkanShader>().GetLayoutSets();
			auto& psSet = InPS->GetAs<VulkanShader>().GetLayoutSets();

			// Deferred shading layout
			_setLayoutBindings.clear();

			MergeBindingSet(vsSet, _setLayoutBindings);
			MergeBindingSet(psSet, _setLayoutBindings);

			for (auto& curSet : _setLayoutBindings)
			{
				std::vector<VkDescriptorSetLayoutBinding>& curBindingSet = curSet.second;

				for (auto& curBinding : curBindingSet)
				{
					if (curBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
					{
						curBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
					}
					else if (curBinding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
					{
						curBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
					}
				}
			}

			_descriptorSetLayouts.resize(_setLayoutBindings.size());
			
			// step through sets numerically
			for (int32_t Iter = 0, foundIdx = 0; Iter < 4; Iter++)
			{
				auto foundSet = _setLayoutBindings.find(Iter);
				if(foundSet != _setLayoutBindings.end())
				{
					VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(foundSet->second);
					VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &_descriptorSetLayouts[foundIdx]));
					
					if (!foundIdx)
					{
						_startSetIdx = Iter;
					}

					foundIdx++;
				}
			}

			SE_ASSERT(InVS->GetAs<VulkanShader>().GetModule());
			SE_ASSERT(InPS->GetAs<VulkanShader>().GetModule());

			shaderStages.push_back({ 
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				nullptr,
				VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT,
				VK_SHADER_STAGE_VERTEX_BIT,
				InVS->GetAs<VulkanShader>().GetModule(),
				InVS->GetAs<VulkanShader>().GetEntryPoint().c_str()
				});
			shaderStages.push_back({
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				nullptr,
				VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				InPS->GetAs<VulkanShader>().GetModule(),
				InPS->GetAs<VulkanShader>().GetEntryPoint().c_str()
				});
			
			// Create the pipeline layout that is used to generate the rendering pipelines that are based on this descriptor set layout
			// In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused
			VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
			pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pPipelineLayoutCreateInfo.pNext = nullptr;
			pPipelineLayoutCreateInfo.setLayoutCount = _descriptorSetLayouts.size();
			pPipelineLayoutCreateInfo.pSetLayouts = _descriptorSetLayouts.data();

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
			//VkPipelineCache pipelineCache;
			//VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
			//pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			//VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

			// Create rendering pipeline using the specified states
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &_pipeline));
		}
		else if(InCS)
		{
			auto& csShaderSets = InCS->GetAs<VulkanShader>().GetLayoutSets();

			MergeBindingSet(csShaderSets, _setLayoutBindings);

			for (auto& curSet : _setLayoutBindings)
			{
				std::vector<VkDescriptorSetLayoutBinding>& curBindingSet = curSet.second;

				for (auto& curBinding : curBindingSet)
				{
					if (curBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
					{
						curBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
					}
					else if (curBinding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
					{
						curBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
					}
				}
			}

			_descriptorSetLayouts.resize(_setLayoutBindings.size());

			// step through sets numerically
			for (int32_t Iter = 0, foundIdx = 0; Iter < 4; Iter++)
			{
				auto foundSet = _setLayoutBindings.find(Iter);
				if (foundSet != _setLayoutBindings.end())
				{
					VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(foundSet->second);
					VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &_descriptorSetLayouts[foundIdx]));

					if (!foundIdx)
					{
						_startSetIdx = Iter;
					}

					foundIdx++;
				}
			}

			VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
			pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pPipelineLayoutCreateInfo.pNext = nullptr;
			pPipelineLayoutCreateInfo.setLayoutCount = _descriptorSetLayouts.size();
			pPipelineLayoutCreateInfo.pSetLayouts = _descriptorSetLayouts.data();

			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &_pipelineLayout));

			VkComputePipelineCreateInfo computePipelineCreateInfo =	vks::initializers::computePipelineCreateInfo(_pipelineLayout, 0);
			computePipelineCreateInfo.stage =
				{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				nullptr,
				VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT,
				VK_SHADER_STAGE_COMPUTE_BIT,
				InCS->GetAs<VulkanShader>().GetModule(),
				InCS->GetAs<VulkanShader>().GetEntryPoint().c_str()
				};

			VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &_pipeline));
		}
		else
		{
			SE_ASSERT(false);
		}		
	}


	bool VulkanPipelineStateKey::operator<(const VulkanPipelineStateKey& compareKey)const
	{
		if (blendState != compareKey.blendState)
		{
			return blendState < compareKey.blendState;
		}
		if (rasterizerState != compareKey.rasterizerState)
		{
			return rasterizerState < compareKey.rasterizerState;
		}
		if (depthState != compareKey.depthState)
		{
			return depthState < compareKey.depthState;
		}
		if (topology != compareKey.topology)
		{
			return topology < compareKey.topology;
		}

		if (inputLayout != compareKey.inputLayout)
		{
			return inputLayout < compareKey.inputLayout;
		}

		if (vs != compareKey.vs)
		{
			return vs < compareKey.vs;
		}
		if (ps != compareKey.ps)
		{
			return ps < compareKey.ps;
		}
		if (ms != compareKey.ms)
		{
			return ms < compareKey.ms;
		}
		if (as != compareKey.as)
		{
			return as < compareKey.as;
		}
		if (hs != compareKey.hs)
		{
			return hs < compareKey.hs;
		}
		if (ds != compareKey.ds)
		{
			return ds < compareKey.ds;
		}
		if (cs != compareKey.cs)
		{
			return cs < compareKey.cs;
		}

		return false;
	}

	GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(GraphicsDevice* InOwner,
		EBlendState InBlendState,
		ERasterizerState InRasterizerState,
		EDepthState InDepthState,
		EDrawingTopology InTopology,
		GPUReferencer< GPUInputLayout > InLayout,
		GPUReferencer< GPUShader> InVS,
		GPUReferencer< GPUShader> InPS,
		GPUReferencer< GPUShader> InMS,
		GPUReferencer< GPUShader> InAS,
		GPUReferencer< GPUShader> InHS,
		GPUReferencer< GPUShader> InDS,
		GPUReferencer< GPUShader> InCS)
	{
		VulkanPipelineStateKey key{ InBlendState, InRasterizerState, InDepthState, InTopology,
			(uintptr_t)InLayout.get(),
			(uintptr_t)InVS.get(),
			(uintptr_t)InPS.get(),
			(uintptr_t)InMS.get(),
			(uintptr_t)InAS.get(),
			(uintptr_t)InHS.get(),
			(uintptr_t)InDS.get(),
			(uintptr_t)InCS.get() };

		auto vulkanGraphicsDevice = dynamic_cast<VulkanGraphicsDevice*>(InOwner);
		auto& PipelineStateMap = vulkanGraphicsDevice->GetPipelineStateMap();

		auto findKey = PipelineStateMap.find(key);

		if (findKey == PipelineStateMap.end())
		{
			auto newPipelineState = Make_GPU(VulkanPipelineState, InOwner);
			newPipelineState->Initialize(InBlendState, InRasterizerState, InDepthState, InTopology, InLayout, InVS, InPS, InMS, InAS, InHS, InDS, InCS);
			PipelineStateMap[key] = newPipelineState;
			return newPipelineState;
		}

		return findKey->second;
	}


	GPUReferencer< class GPUShader > VulkanGraphicsDevice::_gxCreateShader(EShaderType InType)
	{
		return Make_GPU(VulkanShader, this, InType);
	}

	GPUReferencer< class GPUBuffer > VulkanGraphicsDevice::_gxCreateBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData)
	{
		return Vulkan_CreateStaticBuffer(this, InType, InCpuData);
	}

	GPUReferencer< class GPUTexture > VulkanGraphicsDevice::_gxCreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo) 
	{
		return Make_GPU(VulkanTexture, this, Width, Height, Format, RawData, InMetaInfo);
	}

	std::shared_ptr< class GD_Texture > VulkanGraphicsDevice::CreateTexture()
	{
		return std::make_shared< GD_Texture>(this);
	}
	std::shared_ptr< class GD_Shader > VulkanGraphicsDevice::CreateShader()
	{
		return std::make_shared< GD_Shader>(this);
	}
	std::shared_ptr< class GD_Buffer > VulkanGraphicsDevice::CreateBuffer(GPUBufferType InType)
	{
		return std::make_shared< GD_Buffer>(this);
	}
	
	
	//virtual GPUReferencer< GPUTexture > CreateTexture(int32_t Width,
	//	int32_t Height,
	//	TextureFormat Format,
	//	std::shared_ptr< ArrayResource > RawData = nullptr,
	//	std::shared_ptr< ImageMeta > InMetaInfo = nullptr) override
	//{
	//	return Vulkan_CreateTexture(Width, Height, Format, RawData, InMetaInfo);
	//}
	//virtual GPUReferencer< GPURenderTarget > CreateRenderTarget(int32_t Width, int32_t Height, TextureFormat Format) override
	//{
	//	return nullptr;
	//}
	//virtual std::shared_ptr< GD_ComputeDispatch > CreateComputeDispatch(GPUReferencer< GPUShader> InCS) override
	//{
	//	return nullptr;
	//}
	//virtual std::shared_ptr<GD_RenderScene> CreateRenderScene() override
	//{
	//	return std::make_shared< VulkanRenderScene >();
	//}
	//virtual std::shared_ptr<GD_RenderableMesh> CreateRenderableMesh() override
	//{
	//	return nullptr;
	//}
	//virtual std::shared_ptr<GD_RenderableSignedDistanceField> CreateRenderableSDF(GD_RenderableSignedDistanceField::Args&& InArgs) override
	//{
	//	return Vulkan_CreateSDF(InArgs);
	//}
	
}

namespace vks
{
	/**
	* Default constructor
	*
	* @param physicalDevice Physical device that is to be used
	*/
	VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice)
	{
		assert(physicalDevice);
		this->physicalDevice = physicalDevice;

		// Store Properties features, limits and properties of the physical device for later use
		// Device properties also contain limits and sparse properties
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		// Features should be checked by the examples before using them
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);
		// Memory properties are used regularly for creating all kinds of buffers
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
		// Queue family properties, used for setting up requested queues upon device creation
		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		assert(queueFamilyCount > 0);
		queueFamilyProperties.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

		// Get list of supported extensions
		uint32_t extCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
		if (extCount > 0)
		{
			std::vector<VkExtensionProperties> extensions(extCount);
			if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
			{
				for (auto ext : extensions)
				{
					supportedExtensions.push_back(ext.extensionName);
				}
			}
		}
	}

	/**
	* Default destructor
	*
	* @note Frees the logical device
	*/
	VulkanDevice::~VulkanDevice()
	{
		if (commandPool)
		{
			vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
		}
		if (logicalDevice)
		{
			vkDestroyDevice(logicalDevice, nullptr);
		}
	}

	/**
	* Get the index of a memory type that has all the requested property bits set
	*
	* @param typeBits Bit mask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
	* @param properties Bit mask of properties for the memory type to request
	* @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
	*
	* @return Index of the requested memory type
	*
	* @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
	*/
	uint32_t VulkanDevice::getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound) const
	{
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					if (memTypeFound)
					{
						*memTypeFound = true;
					}
					return i;
				}
			}
			typeBits >>= 1;
		}

		if (memTypeFound)
		{
			*memTypeFound = false;
			return 0;
		}
		else
		{
			throw std::runtime_error("Could not find a matching memory type");
		}
	}

	/**
	* Get the index of a queue family that supports the requested queue flags
	*
	* @param queueFlags Queue flags to find a queue family index for
	*
	* @return Index of the queue family index that matches the flags
	*
	* @throw Throws an exception if no queue family index could be found that supports the requested flags
	*/
	uint32_t VulkanDevice::getQueueFamilyIndex(VkQueueFlagBits queueFlags) const
	{
		// Dedicated queue for compute
		// Try to find a queue family index that supports compute but not graphics
		if (queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
			{
				if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
				{
					return i;
				}
			}
		}

		// Dedicated queue for transfer
		// Try to find a queue family index that supports transfer but not graphics and compute
		if (queueFlags & VK_QUEUE_TRANSFER_BIT)
		{
			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
			{
				if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
				{
					return i;
				}
			}
		}

		// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if (queueFamilyProperties[i].queueFlags & queueFlags)
			{
				return i;
			}
		}

		throw std::runtime_error("Could not find a matching queue family index");
	}

	/**
	* Create the logical device based on the assigned physical device, also gets default queue family indices
	*
	* @param enabledFeatures Can be used to enable certain features upon device creation
	* @param pNextChain Optional chain of pointer to extension structures
	* @param useSwapChain Set to false for headless rendering to omit the swapchain device extensions
	* @param requestedQueueTypes Bit flags specifying the queue types to be requested from the device
	*
	* @return VkResult of the device creation call
	*/
	VkResult VulkanDevice::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes)
	{
		// Desired queues need to be requested upon logical device creation
		// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
		// requests different queue types

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

		// Get queue family indices for the requested queue family types
		// Note that the indices may overlap depending on the implementation

		const float defaultQueuePriority(0.0f);

		// Graphics queue
		if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
		{
			queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
		else
		{
			queueFamilyIndices.graphics = 0;
		}

		// Dedicated compute queue
		if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
		{
			queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
			if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
			{
				// If compute family index differs, we need an additional queue create info for the compute queue
				VkDeviceQueueCreateInfo queueInfo{};
				queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
				queueInfo.queueCount = 1;
				queueInfo.pQueuePriorities = &defaultQueuePriority;
				queueCreateInfos.push_back(queueInfo);
			}
		}
		else
		{
			// Else we use the same queue
			queueFamilyIndices.compute = queueFamilyIndices.graphics;
		}

		// Dedicated transfer queue
		if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
		{
			queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
			if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute))
			{
				// If compute family index differs, we need an additional queue create info for the compute queue
				VkDeviceQueueCreateInfo queueInfo{};
				queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
				queueInfo.queueCount = 1;
				queueInfo.pQueuePriorities = &defaultQueuePriority;
				queueCreateInfos.push_back(queueInfo);
			}
		}
		else
		{
			// Else we use the same queue
			queueFamilyIndices.transfer = queueFamilyIndices.graphics;
		}

		// Create the logical device representation
		std::vector<const char*> deviceExtensions(enabledExtensions);
		if (useSwapChain)
		{
			// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
			deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.pEnabledFeatures = &enabledFeatures;		

		// If a pNext(Chain) has been passed, we need to add it to the device creation info
		VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
		if (pNextChain) {
			physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			physicalDeviceFeatures2.features = enabledFeatures;
			physicalDeviceFeatures2.pNext = pNextChain;
			deviceCreateInfo.pEnabledFeatures = nullptr;
			deviceCreateInfo.pNext = &physicalDeviceFeatures2;
		}

		// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
		if (extensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
		{
			deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
			enableDebugMarkers = true;
		}

		if (deviceExtensions.size() > 0)
		{
			for (const char* enabledExtension : deviceExtensions)
			{
				if (!extensionSupported(enabledExtension)) {
					std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
				}
			}

			deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
			deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
		}

		this->enabledFeatures = enabledFeatures;

		VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);
		if (result != VK_SUCCESS)
		{
			return result;
		}

		// Create a default command pool for graphics command buffers
		commandPool = createCommandPool(queueFamilyIndices.graphics);

		return result;
	}

	/**
	* Create a buffer on the device
	*
	* @param usageFlags Usage flag bit mask for the buffer (i.e. index, vertex, uniform buffer)
	* @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
	* @param size Size of the buffer in byes
	* @param buffer Pointer to the buffer handle acquired by the function
	* @param memory Pointer to the memory handle acquired by the function
	* @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
	*
	* @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
	*/
	VkResult VulkanDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data)
	{
		// Create the buffer handle
		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo(usageFlags, size);
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, buffer));

		// Create the memory backing up the buffer handle
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		// Find a memory type index that fits the properties of the buffer
		memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
		// If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
		VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
		if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
			allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
			allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
			memAlloc.pNext = &allocFlagsInfo;
		}
		VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, memory));

		// If a pointer to the buffer data has been passed, map the buffer and copy over the data
		if (data != nullptr)
		{
			void* mapped;
			VK_CHECK_RESULT(vkMapMemory(logicalDevice, *memory, 0, size, 0, &mapped));
			memcpy(mapped, data, size);
			// If host coherency hasn't been requested, do a manual flush to make writes visible
			if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			{
				VkMappedMemoryRange mappedRange = vks::initializers::mappedMemoryRange();
				mappedRange.memory = *memory;
				mappedRange.offset = 0;
				mappedRange.size = size;
				vkFlushMappedMemoryRanges(logicalDevice, 1, &mappedRange);
			}
			vkUnmapMemory(logicalDevice, *memory);
		}

		// Attach the memory to the buffer object
		VK_CHECK_RESULT(vkBindBufferMemory(logicalDevice, *buffer, *memory, 0));

		return VK_SUCCESS;
	}

	/**
	* Create a buffer on the device
	*
	* @param usageFlags Usage flag bit mask for the buffer (i.e. index, vertex, uniform buffer)
	* @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
	* @param buffer Pointer to a vk::Vulkan buffer object
	* @param size Size of the buffer in bytes
	* @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
	*
	* @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
	*/
	//VkResult VulkanDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size, void* data)
	//{
	//	buffer->device = logicalDevice;

	//	// Create the buffer handle
	//	VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo(usageFlags, size);
	//	VK_CHECK_RESULT(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer));

	//	// Create the memory backing up the buffer handle
	//	VkMemoryRequirements memReqs;
	//	VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
	//	vkGetBufferMemoryRequirements(logicalDevice, buffer->buffer, &memReqs);
	//	memAlloc.allocationSize = memReqs.size;
	//	// Find a memory type index that fits the properties of the buffer
	//	memAlloc.memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
	//	// If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
	//	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
	//	if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
	//		allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
	//		allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	//		memAlloc.pNext = &allocFlagsInfo;
	//	}
	//	VK_CHECK_RESULT(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &buffer->memory));

	//	buffer->alignment = memReqs.alignment;
	//	buffer->size = size;
	//	buffer->usageFlags = usageFlags;
	//	buffer->memoryPropertyFlags = memoryPropertyFlags;

	//	// If a pointer to the buffer data has been passed, map the buffer and copy over the data
	//	if (data != nullptr)
	//	{
	//		VK_CHECK_RESULT(buffer->map());
	//		memcpy(buffer->mapped, data, size);
	//		if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
	//			buffer->flush();

	//		buffer->unmap();
	//	}

	//	// Initialize a default descriptor that covers the whole buffer size
	//	buffer->setupDescriptor();

	//	// Attach the memory to the buffer object
	//	return buffer->bind();
	//}

	/**
	* Copy buffer data from src to dst using VkCmdCopyBuffer
	*
	* @param src Pointer to the source buffer to copy from
	* @param dst Pointer to the destination buffer to copy to
	* @param queue Pointer
	* @param copyRegion (Optional) Pointer to a copy region, if NULL, the whole buffer is copied
	*
	* @note Source and destination pointers must have the appropriate transfer usage flags set (TRANSFER_SRC / TRANSFER_DST)
	*/
	/*void VulkanDevice::copyBuffer(vks::Buffer* src, vks::Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion)
	{
		assert(dst->size <= src->size);
		assert(src->buffer);
		VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy bufferCopy{};
		if (copyRegion == nullptr)
		{
			bufferCopy.size = src->size;
		}
		else
		{
			bufferCopy = *copyRegion;
		}

		vkCmdCopyBuffer(copyCmd, src->buffer, dst->buffer, 1, &bufferCopy);

		flushCommandBuffer(copyCmd, queue);
	}*/

	/**
	* Create a command pool for allocation command buffers from
	*
	* @param queueFamilyIndex Family index of the queue to create the command pool for
	* @param createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
	*
	* @note Command buffers allocated from the created pool can only be submitted to a queue with the same family index
	*
	* @return A handle to the created command buffer
	*/
	VkCommandPool VulkanDevice::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
	{
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
		cmdPoolInfo.flags = createFlags;
		VkCommandPool cmdPool;
		VK_CHECK_RESULT(vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
		return cmdPool;
	}

	/**
	* Allocate a command buffer from the command pool
	*
	* @param level Level of the new command buffer (primary or secondary)
	* @param pool Command pool from which the command buffer will be allocated
	* @param (Optional) begin If true, recording on the new command buffer will be started (vkBeginCommandBuffer) (Defaults to false)
	*
	* @return A handle to the allocated command buffer
	*/
	VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin)
	{
		VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(pool, level, 1);
		VkCommandBuffer cmdBuffer;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));
		// If requested, also start recording for the new command buffer
		if (begin)
		{
			VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
			VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
		}
		return cmdBuffer;
	}

	VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, bool begin)
	{
		return createCommandBuffer(level, commandPool, begin);
	}

	/**
	* Finish command buffer recording and submit it to a queue
	*
	* @param commandBuffer Command buffer to flush
	* @param queue Queue to submit the command buffer to
	* @param pool Command pool on which the command buffer has been created
	* @param free (Optional) Free the command buffer once it has been submitted (Defaults to true)
	*
	* @note The queue that the command buffer is submitted to must be from the same family index as the pool it was allocated from
	* @note Uses a fence to ensure command buffer has finished executing
	*/
	void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free)
	{
		if (commandBuffer == VK_NULL_HANDLE)
		{
			return;
		}

		VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = vks::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		// Create fence to ensure that the command buffer has finished executing
		VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
		VkFence fence;
		VK_CHECK_RESULT(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence));
		// Submit to the queue
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
		// Wait for the fence to signal that command buffer has finished executing
		VK_CHECK_RESULT(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
		vkDestroyFence(logicalDevice, fence, nullptr);
		if (free)
		{
			vkFreeCommandBuffers(logicalDevice, pool, 1, &commandBuffer);
		}
	}

	void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
	{
		return flushCommandBuffer(commandBuffer, queue, commandPool, free);
	}

	/**
	* Check if an extension is supported by the (physical device)
	*
	* @param extension Name of the extension to check
	*
	* @return True if the extension is supported (present in the list read at device creation time)
	*/
	bool VulkanDevice::extensionSupported(std::string extension)
	{
		return (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end());
	}

	/**
	* Select the best-fit depth format for this device from a list of possible depth (and stencil) formats
	*
	* @param checkSamplingSupport Check if the format can be sampled from (e.g. for shader reads)
	*
	* @return The depth format that best fits for the current device
	*
	* @throw Throws an exception if no depth format fits the requirements
	*/
	VkFormat VulkanDevice::getSupportedDepthFormat(bool checkSamplingSupport)
	{
		// All depth formats may be optional, so we need to find a suitable depth format to use
		std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
		for (auto& format : depthFormats)
		{
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
			// Format must support depth stencil attachment for optimal tiling
			if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			{
				if (checkSamplingSupport) {
					if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
						continue;
					}
				}
				return format;
			}
		}
		throw std::runtime_error("Could not find a matching depth format");
	}

};
