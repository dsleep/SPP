/*
* Vulkan device class
*
* Encapsulates a physical Vulkan device and its logical representation
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "SPPCore.h"
#include "SPPString.h"
#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"
#include "SPPCamera.h"

#include "VulkanSwapChain.h"

#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include "vulkan/vulkan.h"
#include <algorithm>
#include <assert.h>
#include <exception>

namespace vks
{
	struct VulkanDevice;
}

namespace SPP
{
	struct VulkanPipelineStateKey
	{
		EBlendState blendState = EBlendState::Disabled;
		ERasterizerState rasterizerState = ERasterizerState::BackFaceCull;
		EDepthState depthState = EDepthState::Enabled;
		EDrawingTopology topology = EDrawingTopology::TriangleList;

		uintptr_t inputLayout = 0;

		uintptr_t vs = 0;
		uintptr_t ps = 0;
		uintptr_t ms = 0;
		uintptr_t as = 0;
		uintptr_t hs = 0;
		uintptr_t ds = 0;
		uintptr_t cs = 0;

		bool operator<(const VulkanPipelineStateKey& compareKey)const;
	};

	class VulkanInputLayout : public GPUInputLayout
	{
	private:
		std::vector<VkVertexInputAttributeDescription> _vertexInputAttributes;
		VkPipelineVertexInputStateCreateInfo _vertexInputState;

	public:
		VkPipelineVertexInputStateCreateInfo& GetVertexInputState();
		virtual void UploadToGpu() override {}
		virtual ~VulkanInputLayout() {};

		std::vector< VkVertexInputBindingDescription > InputBindings;
		std::vector< VkVertexInputAttributeDescription > InputAttributes;

		template <typename T, typename... Args>
		void AddVertexAttribute(uintptr_t baseAddr, const T& first, const Args&... args)
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

			Desc.binding = (uint32_t)InputBindings.size();
			Desc.location = (uint32_t)InputAttributes.size();
			Desc.offset = (uint32_t)(reinterpret_cast<uintptr_t>(&first) - baseAddr);

			// todo real proper check
			SE_ASSERT(Desc.offset < 128);

			InputAttributes.push_back(Desc);

			if constexpr (sizeof...(args) >= 1)
			{
				AddVertexAttribute(baseAddr, args...);
			}
		}

		template<class VertexType, typename... VertexMembers>
		void AddVertexStream(const VertexType& InType, const VertexMembers&... inMembers)
		{
			VkVertexInputBindingDescription vertexInputBinding = {};
			vertexInputBinding.binding = (uint32_t)InputBindings.size();
			vertexInputBinding.stride = sizeof(VertexType);
			vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			AddVertexAttribute(reinterpret_cast<uintptr_t>(&InType), inMembers...);

			InputBindings.push_back(vertexInputBinding);
		}
		void Begin()
		{
			InputBindings.clear();
			InputAttributes.clear();
		}
		void Finalize();
		virtual void InitializeLayout(const std::vector< InputLayoutElement>& eleList) override;
	};

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

		PerFrameStagingBuffer _perFrameScratchBuffer;


	

		// Active frame buffer index
		uint32_t currentBuffer = 0;
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
		vks::VulkanDevice* vulkanDevice = nullptr;

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


		VkResult createInstance(bool enableValidation);
		bool DeviceInitialize();
		void nextFrame();
		void updateOverlay();
		void createPipelineCache();
		void createCommandPool();
		void createSynchronizationPrimitives();
		void initSwapchain();
		void setupSwapChain();
		void createCommandBuffers();
		void destroyCommandBuffers();
		void setupDepthStencil();
		void setupRenderPass();
		void setupFrameBuffer();

		void CreateDescriptorPool();

	public:
		vks::VulkanDevice* GetVKSVulkanDevice() {
			return vulkanDevice;
		}


		VkFramebuffer GetActiveFrameBuffer()
		{
			return frameBuffers[currentBuffer];
		}

		uint8_t GetActiveFrame()
		{
			return (uint8_t)currentBuffer;
		}
		uint8_t GetInFlightFrames()
		{
			return (uint8_t)swapChain.imageCount;
		}

		VkCommandBuffer& GetActiveCommandBuffer()
		{
			return drawCmdBuffers[currentBuffer];
		}

		PerFrameStagingBuffer& GetPerFrameScratchBuffer()
		{
			return _perFrameScratchBuffer;
		}

		VkDevice GetDevice();
		VkRenderPass GetBaseRenderPass();
		virtual void Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow);
		virtual void ResizeBuffers(int32_t NewWidth, int32_t NewHeight);
		virtual int32_t GetDeviceWidth() const;
		virtual int32_t GetDeviceHeight() const;
		void CreateInputLayout(GPUReferencer < GPUInputLayout > InLayout);
		
		Vector2i GetExtents() const
		{
			return Vector2i(width, height);
		};


		virtual void BeginFrame() override;
		virtual void EndFrame() override;
		virtual void MoveToNextFrame();
	};

	class VulkanPipelineState : public PipelineState
	{
	private:
		// The pipeline layout is used by a pipeline to access the descriptor sets
		// It defines interface (without binding any actual data) between the shader stages used by the pipeline and the shader resources
		// A pipeline layout can be shared among multiple pipelines as long as their interfaces match
		VkPipelineLayout _pipelineLayout = nullptr;

		// The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
		// Like the pipeline layout it's pretty much a blueprint and can be used with different descriptor sets as long as their layout matches
		VkDescriptorSetLayout _descriptorSetLayout = nullptr;

		// Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
		// While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
		// So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
		// Even though this adds a new dimension of planing ahead, it's a great opportunity for performance optimizations by the driver
		VkPipeline _pipeline = nullptr;

	public:
		VulkanPipelineState();
		virtual ~VulkanPipelineState();

		VkPipeline GetVkPipeline()
		{
			return _pipeline;
		}
		VkPipelineLayout GetVkPipelineLayout()
		{
			return _pipelineLayout;
		}

		virtual const char* GetName() const { return "VulkanPipelineState"; }
		virtual void UploadToGpu() {}

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

			GPUReferencer< GPUShader> InCS);
	};

	GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(EBlendState InBlendState,
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
		GPUReferencer< GPUShader> InCS);
}

namespace vks
{
	struct VulkanDevice
	{
		/** @brief Physical device representation */
		VkPhysicalDevice physicalDevice;
		/** @brief Logical device representation (application's view of the device) */
		VkDevice logicalDevice;
		/** @brief Properties of the physical device including limits that the application can check against */
		VkPhysicalDeviceProperties properties;
		/** @brief Features of the physical device that an application can use to check if a feature is supported */
		VkPhysicalDeviceFeatures features;
		/** @brief Features that have been enabled for use on the physical device */
		VkPhysicalDeviceFeatures enabledFeatures;
		/** @brief Memory types and heaps of the physical device */
		VkPhysicalDeviceMemoryProperties memoryProperties;
		/** @brief Queue family properties of the physical device */
		std::vector<VkQueueFamilyProperties> queueFamilyProperties;
		/** @brief List of extensions supported by the device */
		std::vector<std::string> supportedExtensions;
		/** @brief Default command pool for the graphics queue family index */
		VkCommandPool commandPool = VK_NULL_HANDLE;
		/** @brief Set to true when the debug marker extension is detected */
		bool enableDebugMarkers = false;
		/** @brief Contains queue family indices */
		struct
		{
			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
		} queueFamilyIndices;
		operator VkDevice() const
		{
			return logicalDevice;
		};
		explicit VulkanDevice(VkPhysicalDevice physicalDevice);
		~VulkanDevice();
		uint32_t        getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr) const;
		uint32_t        getQueueFamilyIndex(VkQueueFlagBits queueFlags) const;
		VkResult        createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
		VkResult        createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory, void* data = nullptr);
		//VkResult        createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, vks::Buffer* buffer, VkDeviceSize size, void* data = nullptr);
		//void            copyBuffer(vks::Buffer* src, vks::Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion = nullptr);
		VkCommandPool   createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = false);
		VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false);
		void            flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
		void            flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);
		bool            extensionSupported(std::string extension);
		VkFormat        getSupportedDepthFormat(bool checkSamplingSupport);

		auto GetMinUniformBufferOffsetAlignment()
		{
			return properties.limits.minUniformBufferOffsetAlignment;
		}
	};
}        // namespace vks
