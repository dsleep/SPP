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
#include "VulkanFrameBuffer.hpp"
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

#define MAX_IN_FLIGHT 3

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

	_declspec(align(256u)) struct StaticDrawParams
	{
		//altered viewposition translated
		Matrix4x4 LocalToWorldScaleRotation;
		Vector3d Translation;
		uint32_t MaterialID;
	};

	template<typename IndexedData>
	class FrameTrackedLeaseManager : public LeaseManager< IndexedData, uint8_t >
	{
	protected:
		using LM = LeaseManager< IndexedData, uint8_t >;

		struct Purged
		{
			BitReference _bitRef;
			uint8_t _tag;
		};
		std::list< Purged > _pendingPurge;

	public:
		FrameTrackedLeaseManager(IndexedData& InIndexor) : LM(InIndexor)
		{

		}

		virtual void EndLease(typename LM::Lease& InLease) override
		{
			_pendingPurge.push_back({ std::move(InLease.GetBitReference()), InLease.GetTag() });
		}

		void ClearTag(uint8_t InTag)
		{

		}

		void ClearAll()
		{
			_pendingPurge.empty();
		}
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
		
		/** @brief Pipeline stages used to wait at for graphics queue submissions */
		VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// Contains command buffers and semaphores to be presented to the queue
		VkSubmitInfo submitInfo;
		// Global render pass for frame buffer writes
		VkRenderPass renderPass = VK_NULL_HANDLE;

		// Command buffer pool
		VkCommandPool cmdPool;

		PerFrameStagingBuffer _perFrameScratchBuffer;

		std::vector<GPUResource*> _cpuPushedDyingResources;
		std::vector<GPUResource*> _gpuPushedDyingResources;
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

		std::unique_ptr<VulkanFramebuffer> _colorTarget;
		std::unique_ptr<VulkanFramebuffer> _deferredMaterialMRTs;
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
		std::shared_ptr<GD_Material> _defaultMaterial;
		std::shared_ptr< class GD_Shader > _meshvertexShader, _meshpixelShader;

		std::shared_ptr< ArrayResource > _staticInstanceDrawInfoCPU;
		TSpan< StaticDrawParams > _staticInstanceDrawInfoSpan;
		GPUReferencer< class VulkanBuffer > _staticInstanceDrawInfoGPU;

		std::unique_ptr< FrameTrackedLeaseManager< TSpan< StaticDrawParams > > > _staticInstanceDrawLeaseManager;

		VkDescriptorPool _globalPool;
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
		void setupRenderPass();

		void setupFrameBuffer();
		void destroyFrameBuffer();

		void CreateDescriptorPool();

	public:
		VulkanGraphicsDevice()
		{
		}

		vks::VulkanDevice* GetVKSVulkanDevice() {
			return vulkanDevice;
		}

		VkQueue GetDeviceQueue() {
			return queue;
		}

		VkFramebuffer GetActiveFrameBuffer()
		{
			return _frameBuffers[currentBuffer]->Get();
		}

		VkDescriptorPool GetActiveDescriptorPool()
		{
			return _perDrawPools[currentBuffer];
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
			return _drawCmdBuffers[currentBuffer]->Get();
		}

		VkCommandBuffer& GetCopyCommandBuffer();

		std::map< VulkanPipelineStateKey, GPUReferencer< VulkanPipelineState > >& GetPipelineStateMap()
		{
			return _piplineStateMap;
		}

		PerFrameStagingBuffer& GetPerFrameScratchBuffer()
		{
			return _perFrameScratchBuffer;
		}

		GPUReferencer< GPUTexture > GetDefaultTexture()
		{
			return _defaultTexture;
		}

		VkRenderPass GetBackBufferRenderPass()
		{
			return renderPass;
		}

		VkFrameData GetColorFrameData()
		{
			return _colorTarget->GetFrameData();
		}

		VkDescriptorImageInfo GetColorImageDescImgInfo()
		{
			return _colorTarget->GetImageInfo();
		}

		VulkanFramebuffer *GetColorTarget()
		{
			return _colorTarget.get();
		}

		VkFrameData GetBackBufferFrameData()
		{
			return VkFrameData{ renderPass, GetActiveFrameBuffer() };
		}

		VkDevice GetDevice();
		VkRenderPass GetBaseRenderPass();
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
		virtual GPUReferencer< class GPUBuffer > _gxCreateBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData = nullptr) override;

		//
		virtual std::shared_ptr< class GD_Texture > CreateTexture() override;
		virtual std::shared_ptr< class GD_Shader > CreateShader() override;
		virtual std::shared_ptr< class GD_Buffer > CreateBuffer(GPUBufferType InType) override;

		virtual std::shared_ptr< class GD_Material > CreateMaterial() override;
		virtual std::shared_ptr< class GD_RenderScene > CreateRenderScene() override;

		virtual std::shared_ptr< class GD_RenderableMesh > CreateRenderableMesh() override;

		virtual std::shared_ptr< class GD_Material> GetDefaultMaterial() override;

		virtual std::shared_ptr< class GD_StaticMesh > CreateStaticMesh() override;
		virtual std::shared_ptr< class GD_RenderableSignedDistanceField > CreateSignedDistanceField() override;

		virtual void DrawDebugText(const Vector2i& InPosition, const char* Text, const Color3& InColor = Color3(255, 255, 255)) override;
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
		std::vector<VkDescriptorSetLayout> _descriptorSetLayouts;
		uint8_t _startSetIdx = 0;

		// Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
		// While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
		// So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
		// Even though this adds a new dimension of planing ahead, it's a great opportunity for performance optimizations by the driver
		VkPipeline _pipeline = nullptr;

		VkRenderPass _renderPass = nullptr;

		// for debugging or just leave in?!
		std::map<uint8_t, std::vector<VkDescriptorSetLayoutBinding> > _setLayoutBindings;

		virtual void _MakeResident() override {}
		virtual void _MakeUnresident() override {}

	public:
		VulkanPipelineState(GraphicsDevice* InOwner);
		virtual ~VulkanPipelineState();

		void ManualSetRenderPass(VkRenderPass InRenderPass)
		{
			_renderPass = InRenderPass;
		}

		const VkPipeline &GetVkPipeline()
		{
			return _pipeline;
		}
		auto GetStartIdx() const
		{
			return _startSetIdx;
		}
		const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts()
		{
			return _descriptorSetLayouts;
		}
		const VkPipelineLayout &GetVkPipelineLayout()
		{
			return _pipelineLayout;
		}
		const std::map<uint8_t, std::vector<VkDescriptorSetLayoutBinding> >& GetDescriptorSetLayoutBindings()
		{
			return _setLayoutBindings;
		}

		virtual const char* GetName() const { return "VulkanPipelineState"; }

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
