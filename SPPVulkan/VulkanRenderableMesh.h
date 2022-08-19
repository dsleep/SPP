// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPVulkan.h"

#include "SPPGraphics.h"
#include "SPPSceneRendering.h"


#include "VulkanDevice.h"

namespace SPP
{
	class RT_VulkanStaticMesh : public RT_StaticMesh
	{
		CLASS_RT_RESOURCE();

	protected:
		RT_VulkanStaticMesh(GraphicsDevice* InOwner) : RT_StaticMesh(InOwner) {}

	public:

		virtual void Initialize() override;
		virtual ~RT_VulkanStaticMesh() {}
	};

	class RT_VulkanRenderableMesh : public RT_RenderableMesh
	{
		CLASS_RT_RESOURCE();

	protected:

		GPUReferencer < VulkanPipelineState > _state;
		std::shared_ptr< ArrayResource > _drawConstants;
		GPUReferencer< class VulkanBuffer > _drawConstantsBuffer;

		std::shared_ptr<StaticDrawLeaseManager::Lease> _staticDrawLease;

		bool bPendingUpdate = false;


		RT_VulkanRenderableMesh(GraphicsDevice* InOwner) : RT_RenderableMesh(InOwner) {}
	public:
		virtual ~RT_VulkanRenderableMesh() {}

		virtual void _AddToRenderScene(class RT_RenderScene* InScene) override;
		virtual void _RemoveFromRenderScene() override;

		virtual void PrepareToDraw() override;
		virtual void Draw() override;
	};

	class RT_Vulkan_Material : public RT_Material
	{
		CLASS_RT_RESOURCE();

	protected:
		RT_Vulkan_Material(GraphicsDevice* InOwner) : RT_Material(InOwner) {}

	public:
		virtual ~RT_Vulkan_Material() {}

		GPUReferencer < VulkanPipelineState > GetPipelineState(EDrawingTopology topology,
			GPUReferencer<GPUShader> vsShader,
			GPUReferencer<GPUShader> psShader,
			GPUReferencer<GPUInputLayout> layout);
	};
}