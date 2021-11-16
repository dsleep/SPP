// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "vulkan/vulkan.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	LogEntry LOG_VULKAN("Vulkan");

		
	struct VulkanGraphicInterface : public IGraphicsInterface
	{
		// hacky so one GGI per DLL
		VulkanGraphicInterface()
		{
			SET_GGI(this);
		}

		virtual GPUReferencer< GPUShader > CreateShader(EShaderType InType) override
		{			
			return nullptr;
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
			return nullptr;
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