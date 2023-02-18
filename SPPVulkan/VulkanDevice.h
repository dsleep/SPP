// Copyright(c) David Sleeper(Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
//
// Modified original code from Sascha Willems - www.saschawillems.de

#pragma once

#include "SPPCore.h"
#include "SPPString.h"
#include "SPPGraphics.h"
#include "SPPGPUResources.h"
#include "SPPMath.h"
#include "SPPCamera.h"
#include "SPPBitSetArray.h"
#include "SPPHandledTimers.h"

#include "VulkanResources.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include "vulkan/vulkan.h"

#include "VulkanPipelineState.h"

#include <algorithm>
#include <assert.h>
#include <exception>

namespace vks
{
	struct VulkanDevice;
}

#define MAX_IN_FLIGHT 3

struct VmaAllocator_T;

namespace SPP
{
	
	class VulkanInputLayout : public GPUInputLayout
	{
	private:
		VkPipelineVertexInputStateCreateInfo _vertexInputState;
		std::vector< VkVertexInputBindingDescription > _inputBindings;
		std::vector< VkVertexInputAttributeDescription > _inputAttributes;

		virtual void _MakeResident() override {}
		virtual void _MakeUnresident() override {}

	public:
		VulkanInputLayout(GraphicsDevice* InOwner) : GPUInputLayout(InOwner)
		{
		}


		VkPipelineVertexInputStateCreateInfo& GetVertexInputState();
		virtual ~VulkanInputLayout() 
		{
		};
		virtual void InitializeLayout(const std::vector<VertexStream>& vertexStreams) override;
	};


	struct alignas(64u) StaticDrawParams
	{
		//altered viewposition translated
		Matrix4x4 LocalToWorldScaleRotation;
		Vector3d Translation;
		uint32_t MaterialID;
	};

	template<typename IndexedData>
	class DeviceLeaseManager : public LeaseManager< IndexedData >
	{
	protected:
		using LM = LeaseManager< IndexedData >;

		struct Purged
		{
			// this is the lock that prevents other resources uses it, its non copy moved around until
			// we are ready for it to be accessed on ~BitReference
			BitReference _bitRef;
			uint8_t _tag;
		};
		std::list< Purged > _pendingPurge;

		std::function< void(LM::Reservation&) > _func;
		class VulkanGraphicsDevice* _owner = nullptr;
		
	public:

		DeviceLeaseManager(class VulkanGraphicsDevice* InOwner,
			IndexedData& InIndexor,
			const std::function< void(LM::Reservation&) >& InFunc) :
			_owner(InOwner), _func(InFunc), LM(InIndexor)
		{ }

		virtual void LeaseUpdated(typename LM::Reservation& InLease) override
		{
			_func(InLease);
		}

		virtual void EndLease(typename LM::Reservation& InLease) override
		{
			_pendingPurge.push_back({ std::move(InLease.GetBitReference()), _owner->GetCurrentFrame() });
		}

		void ClearTag(uint8_t InTag)
		{
			for (auto Iter = _pendingPurge.begin(); Iter != _pendingPurge.end();)
			{
				if (Iter->_tag == InTag)
				{
					Iter = _pendingPurge.erase(Iter);
				}
				else
				{
					Iter++;
				}
			}
		}

		void ClearAll()
		{
			_pendingPurge.empty();
		}
	};

	using StaticDrawPoolManager = DeviceLeaseManager< TSpan< StaticDrawParams > >;

	class VulkanGraphicsDevice : public GraphicsDevice
	{
	private:

		struct PrivImpl;
		std::unique_ptr<PrivImpl> _impl;

		PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;

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
		VkQueue graphicsQueue;
		VkQueue computeQueue;
		VkQueue transferQueue;
		VkQueue sparseQueue;

		// Depth buffer format (selected during Vulkan initialization)
		VkFormat depthFormat;
		
		/** @brief Pipeline stages used to wait at for graphics queue submissions */
		VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// Contains command buffers and semaphores to be presented to the queue
		VkSubmitInfo submitInfo;
		// Global render pass for frame buffer writes
		GPUReferencer<SafeVkRenderPass> _backBufferRenderPass;

		// Command buffer pool
		VkCommandPool cmdPool;

		PerFrameStagingBuffer _perFrameScratchBuffer;

		std::vector<GPUResource*> _pendingDyingResources;
		std::array< std::vector<GPUResource*>, MAX_IN_FLIGHT > _dyingResources;
		
		// List of available frame buffers (same as number of swap chain images)
		std::array< GPUReferencer<SafeVkFrameBuffer>, MAX_IN_FLIGHT > _frameBuffers;
		// Command buffers used for rendering
		std::array< GPUReferencer<SafeVkCommandBuffer>, MAX_IN_FLIGHT > _drawCmdBuffers;
		std::array< SafeVkCommandAndFence, MAX_IN_FLIGHT > _copyCmdBuffers;

		std::atomic_bool bDrawPhase{ false };

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

		std::array< GPUReferencer<SafeVkFence>, MAX_IN_FLIGHT > _waitFences;

		struct {
			GPUReferencer<SafeVkImage> image;
			GPUReferencer<SafeVkDeviceMemory> mem;
			GPUReferencer<SafeVkImageView> view;
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

		GPUReferencer< GPUTexture > _defaultTexture;

		std::shared_ptr< ArrayResource > _staticInstanceDrawInfoCPU;
		TSpan< StaticDrawParams > _staticInstanceDrawInfoSpan;
		GPUReferencer< class VulkanBuffer > _staticInstanceDrawInfoGPU;

		std::unique_ptr< StaticDrawPoolManager > _staticInstanceDrawPoolManager;

		VkDescriptorPool _globalPool;

		// free and updates supported
		VkDescriptorPool _sharedGlobalPool;
		std::vector< VkDescriptorPool >  _perDrawPools;
		std::map< VulkanPipelineStateKey, GPUReferencer< class VulkanPipelineState > > _piplineStateMap;

		uint32_t _currentFPSIdx = 0;
		std::vector<float> _FPS;
		STDElapsedTimer _timer;

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
		
		void createStaticDrawInfo();
		void setupBackBufferRenderPass();

		void setupFrameBuffer();
		void destroyFrameBuffer();

		void CreateDescriptorPool();

	public:
		VulkanGraphicsDevice();
		~VulkanGraphicsDevice();

		bool HasCheckPoints() const;

		void SetCheckpoint(VkCommandBuffer InCmdBuffer, const char *InName);

		uint8_t GetCurrentFrame();
		vks::VulkanDevice* GetVKSVulkanDevice();

		VkDevice GetVKDevice() const;

		VkQueue GetGraphicsQueue();

		VkQueue GetComputeQueue();

		VkQueue GetTransferQueue();

		VkQueue GetSparseQueue();

		auto GetStaticDrawPoolReservation()
		{
			return _staticInstanceDrawPoolManager->GetLease();
		}

		VkFramebuffer GetActiveFrameBuffer();

		VkDescriptorPool GetPerFrameResetDescriptorPool();

		VkDescriptorPool GetPersistentDescriptorPool();

		uint8_t GetActiveFrame();
		uint8_t GetInFlightFrames();

		VkCommandBuffer& GetActiveCommandBuffer();
		VkCommandBuffer& GetCopyCommandBuffer();

		VmaAllocator_T *GetVMAAllocator();

		std::map< VulkanPipelineStateKey, GPUReferencer< VulkanPipelineState > >& GetPipelineStateMap();

		PerFrameStagingBuffer& GetPerFrameScratchBuffer();

		auto GetStaticInstanceDrawBuffer()
		{
			return _staticInstanceDrawInfoGPU;
		}

		GPUReferencer< GPUTexture > GetDefaultTexture();
		GPUReferencer< GPUTexture > GetDepthColor();

		struct VkFrameDataContainer& GetMainOpaquePassFrame();
		struct VkFrameDataContainer& GetColorFrameData();
		struct VkFrameDataContainer& GetDeferredFrameData();
		struct VkFrameDataContainer& GetLightingCompositeRenderPass();
		class VulkanFramebuffer* GetLightCompositeFrameBuffer();



		struct VkFrameDataContainer& GetDepthOnlyFrameData();


		uintptr_t _activeRenderPassPTR = 0;
		uintptr_t _activeFrameBufferPTR = 0;
		GPUReferencer<SafeVkFrameBuffer> frameBuffer;

		void SetFrameBufferForRenderPass(struct VkFrameDataContainer& InFrame);
		void ConditionalEndRenderPass();

		VkDescriptorImageInfo GetColorImageDescImgInfo();
		VulkanFramebuffer* GetColorTarget();
		VkFrameDataContainer GetBackBufferFrameData();

		VkDevice GetDevice();
		virtual void Initialize(int32_t InitialWidth, int32_t InitialHeight, void* OSWindow) override;
		virtual void Shutdown() override;
		virtual void ResizeBuffers(int32_t NewWidth, int32_t NewHeight);
		virtual void DyingResource(class GPUResource* InResourceToKill) override;
		virtual int32_t GetDeviceWidth() const;
		virtual int32_t GetDeviceHeight() const;
		void CreateInputLayout(GPUReferencer < GPUInputLayout > InLayout);
		
		Vector2i GetExtents() const
		{
			return Vector2i(width, height);
		};

		void SubmitCopyCommands();

		virtual void SyncGPUData() override;
		virtual void Flush()override;

		virtual void BeginFrame() override;
		virtual void EndFrame() override;
		virtual void MoveToNextFrame();

		virtual GPUReferencer< class GPUShader > _gxCreateShader(EShaderType InType) override;
		
		virtual GPUReferencer< class GPUTexture > _gxCreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData = nullptr, std::shared_ptr< ImageMeta > InMetaInfo = nullptr) override;
		virtual GPUReferencer< class GPUTexture > _gxCreateTexture(const struct TextureAsset& TextureAsset) override;

		virtual GPUReferencer< class GPUBuffer > _gxCreateBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData = nullptr) override;
		virtual GPUReferencer< class GPUBuffer > _gxCreateBuffer(GPUBufferType InType, size_t InSize) override;

		//
		virtual std::shared_ptr< class RT_Texture > CreateTexture() override;
		virtual std::shared_ptr< class RT_Shader > CreateShader() override;
		virtual std::shared_ptr< class RT_Buffer > CreateBuffer(GPUBufferType InType) override;

		virtual std::shared_ptr< class RT_Material > CreateMaterial() override;
		virtual std::shared_ptr< class RT_RenderScene > CreateRenderScene() override;

		virtual std::shared_ptr< class RT_RenderableMesh > CreateRenderableMesh() override;

		virtual std::shared_ptr< class RT_RenderableSVVO > CreateRenderableSVVO() override;

		//virtual std::shared_ptr< class RT_Material> GetDefaultMaterial() override;

		virtual std::shared_ptr< class RT_StaticMesh > CreateStaticMesh() override;
		virtual std::shared_ptr< class RT_RenderableSignedDistanceField > CreateSignedDistanceField() override;

		virtual std::shared_ptr< class RT_SunLight > CreateSunLight() override;
		virtual std::shared_ptr< class RT_PointLight > CreatePointLight() override;

		virtual void DrawDebugText(const Vector2i& InPosition, const char* Text, const Color3& InColor = Color3(255, 255, 255)) override;
	};

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
		std::set<std::string> supportedExtensions;
		/** @brief Default command pool for the graphics queue family index */
		VkCommandPool commandPool = VK_NULL_HANDLE;
		/** @brief Set to true when the debug marker extension is detected */
		bool enableDebugMarkers = false;
		// enable those checkpoints
		bool enableCheckpoints = false;
		/** @brief Contains queue family indices */
		struct
		{
			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
			uint32_t sparse;
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
