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
		RT_VulkanStaticMesh() {}

	public:
		virtual void Initialize() override;
		virtual ~RT_VulkanStaticMesh() {}
	};

	class RT_VulkanRenderableMesh : public RT_RenderableMesh, public IVulkanPassCacher
	{
		CLASS_RT_RESOURCE();

	protected:				
		std::shared_ptr< ArrayResource > _drawConstants;
		GPUReferencer< class VulkanBuffer > _drawConstantsBuffer;

		std::shared_ptr<StaticDrawPoolManager::Reservation> _staticDrawReservation;

		bool bPendingUpdate = false;

		RT_VulkanRenderableMesh() {}
	public:

		auto GetStaticDrawBufferIndex()
		{
			return _staticDrawReservation->GetIndex();
		}
		
		auto GetDrawTransformBuffer()
		{
			return _drawConstantsBuffer;
		}

		virtual ~RT_VulkanRenderableMesh() {}

		virtual void _AddToRenderScene(class RT_RenderScene* InScene) override;
		virtual void _RemoveFromRenderScene() override;

		virtual void PrepareToDraw() override;
		virtual void Draw() override;
	};

	

	class RT_Vulkan_Material : public RT_Material, public IVulkanPassCacher
	{
		CLASS_RT_RESOURCE();

	protected:
		RT_Vulkan_Material() {}

	public:
		virtual ~RT_Vulkan_Material() {}

		GPUReferencer < VulkanPipelineState > GetPipelineState(EDrawingTopology topology,
			GPUReferencer<class VulkanShader> vsShader,
			GPUReferencer<class VulkanShader> psShader,
			GPUReferencer<class VulkanInputLayout> layout);
	};
}