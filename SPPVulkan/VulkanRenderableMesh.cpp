// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"
#include "VulkanDevice.h"
#include "VulkanShaders.h"
#include "VulkanRenderScene.h"
#include "VulkanTexture.h"
#include "SPPGraphics.h"
#include "SPPGraphicsO.h"
#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"


namespace SPP
{	
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;

	extern GPUReferencer< GPUInputLayout > Vulkan_CreateInputLayout();;

	extern GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(EBlendState InBlendState,
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

	class GD_VulkanStaticMesh : public GD_StaticMesh
	{
	protected:
		virtual void _makeResident() override;
		virtual void _makeUnresident() override;

	public:
		GD_VulkanStaticMesh(GraphicsDevice* InOwner) : GD_StaticMesh(InOwner) {}
		virtual ~GD_VulkanStaticMesh() {}
	};
		
	class GD_VulkanRenderableMesh : public GD_RenderableMesh
	{
	protected:

		GPUReferencer < VulkanPipelineState > _state;
		std::shared_ptr< ArrayResource > _drawConstants;
		GPUReferencer< class VulkanBuffer > _drawConstantsBuffer;
		bool bPendingUpdate = false;

	public:
		GD_VulkanRenderableMesh(GraphicsDevice* InOwner) : GD_RenderableMesh(InOwner) {}
		virtual ~GD_VulkanRenderableMesh() {}

		virtual void _AddToRenderScene(class GD_RenderScene* InScene) override;
		virtual void _RemoveFromRenderScene() override;

		virtual void PrepareToDraw() override;
		virtual void Draw() override;
	};

	class GD_Vulkan_Material : public GD_Material
	{
	protected:

	public:
		GD_Vulkan_Material(GraphicsDevice* InOwner) : GD_Material(InOwner) {}
		virtual ~GD_Vulkan_Material() {} 

		GPUReferencer < VulkanPipelineState > GetPipelineState(EDrawingTopology topology,
			GPUReferencer<GPUInputLayout> layout)
		{
			SE_ASSERT(_vertexShader && _pixelShader);

			auto vsRef = _vertexShader->GetGPURef();
			auto psRef = _pixelShader->GetGPURef();

			SE_ASSERT(vsRef && psRef);

			return GetVulkanPipelineState(
				_blendState,
				_rasterizerState,
				_depthState,
				topology,
				layout,
				vsRef,
				psRef,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr);
		}
	};

	std::shared_ptr< class GD_Material > VulkanGraphicsDevice::CreateMaterial()
	{
		return std::make_shared<GD_Vulkan_Material>(this);
	}

	std::shared_ptr< class GD_StaticMesh > VulkanGraphicsDevice::CreateStaticMesh()
	{
		return std::make_shared< GD_VulkanStaticMesh >(this);
	}

	std::shared_ptr< class GD_RenderableMesh > VulkanGraphicsDevice::CreateRenderableMesh()
	{
		return std::make_shared<GD_VulkanRenderableMesh>(this);
	}

	std::shared_ptr< class GD_Material> VulkanGraphicsDevice::GetDefaultMaterial()
	{
		return _defaultMaterial;
	}


	void GD_VulkanStaticMesh::_makeResident()
	{
		GD_StaticMesh::_makeResident();

		_layout = Vulkan_CreateInputLayout();
		_layout->InitializeLayout(_vertexStreams);		
	}


	void GD_VulkanStaticMesh::_makeUnresident()
	{
		GD_StaticMesh::_makeUnresident();
	}

	void GD_VulkanRenderableMesh::_AddToRenderScene(class GD_RenderScene* InScene)
	{
		GD_RenderableMesh::_AddToRenderScene(InScene);

		if (_mesh)
		{
			_mesh->MakeResident();
		}

		_cachedRotationScale = Matrix4x4::Identity();
		_cachedRotationScale.block<3, 3>(0, 0) = GenerateRotationScale();
		
		_drawConstants = std::make_shared< ArrayResource >();
		_drawConstants->InitializeFromType< GPUDrawConstants >(1);		

		bPendingUpdate = true;
	}

	void GD_VulkanRenderableMesh::_RemoveFromRenderScene()
	{
		GD_RenderableMesh::_RemoveFromRenderScene();
	}

	void GD_VulkanRenderableMesh::PrepareToDraw()
	{
		if (!_state)
		{
			if (!_material)
			{
				SE_ASSERT(false);
				_material = _owner->GetDefaultMaterial();
			}

			//auto vulkanMesh = std::dynamic_pointer_cast<GD_VulkanStaticMesh>(_mesh);
			auto vulkanMat = std::dynamic_pointer_cast<GD_Vulkan_Material>(_material);

			SE_ASSERT(vulkanMat);
			_state = vulkanMat->GetPipelineState(_mesh->GetTopology(), _mesh->GetLayout());

			SE_ASSERT(_state);
		}

		if (bPendingUpdate)
		{
			auto uniformData = _drawConstants->GetSpan< GPUDrawConstants>();
			auto& curData = uniformData[0];
			curData.LocalToWorldScaleRotation = _cachedRotationScale;
			curData.Translation = _position;
			_drawConstantsBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Simple, _drawConstants);
			bPendingUpdate = false;
		}
	}

	

	//template<typename F>
	//void NodeTraversal(const Matrix4x4 &InTransform,
	//	uint32_t CurrentIdx, 
	//	const std::vector<MeshNode> &MeshletNodes, 
	//	const Vector3 &InCamPos, 
	//	uint32_t CurrentLevel,
	//	const F &func)
	//{
	//	auto& curNode = MeshletNodes[CurrentIdx];

	//	AABB transformedAABB = curNode.Bounds.Transform(InTransform);
	//	float Radius = transformedAABB.Extent().norm();
	//	float DistanceToCamera = std::max( (HACKS_CameraPos.cast<float>() - transformedAABB.Center()).norm() - Radius, 0.0f);


	//	float DistanceFactor = (DistanceToCamera / 100.0f) * (CurrentLevel+1);

	//	auto StartIdx = std::get<0>(curNode.ChildrenRange);
	//	auto EndIdx = std::get<1>(curNode.ChildrenRange);

	//	auto ChildCount = EndIdx - StartIdx;

	//	if (DistanceFactor < 10.0f && ChildCount > 0)
	//	{
	//		uint32_t StartIdx = std::get<0>(curNode.ChildrenRange);
	//		uint32_t EndIdx = std::get<1>(curNode.ChildrenRange);

	//		for (uint32_t IdxIter = StartIdx; IdxIter < EndIdx; IdxIter++)
	//		{
	//			NodeTraversal(InTransform, IdxIter, MeshletNodes, InCamPos, CurrentLevel+1, func);
	//		}
	//	}
	//	else
	//	{			
	//		func(CurrentIdx);
	//	}
	//}

	//void VulkanRenderableMesh::DrawDebug(std::vector< DebugVertex >& lines)
	//{
		//auto localToWorld = GenerateLocalToWorldMatrix();
		//for (auto _meshData : _meshElements)
		//{
		//	auto CurType = _meshData->GetType();

		//	if (CurType == MeshTypes::Meshlets)
		//	{
		//		auto CurMeshElement = (MeshletedElement*)_meshData.get();

		//		if (CurMeshElement->MeshletNodes.size())
		//		{
		//			NodeTraversal(localToWorld, 0, CurMeshElement->MeshletNodes, HACKS_CameraPos.cast<float>(), 0, [&](uint32_t IdxIter)
		//				{
		//					auto& renderNode = CurMeshElement->MeshletNodes[IdxIter];
		//					auto meshletCount = std::get<1>(renderNode.MeshletRange) - std::get<0>(renderNode.MeshletRange);

		//					auto transformedAABB = renderNode.Bounds.Transform(localToWorld);
		//					DrawAABB(transformedAABB, lines);
		//				});
		//		}
		//	}
		//	else
		//	{
		//		auto sphereBounds = _meshData->Bounds.Transform(localToWorld);
		//		DrawSphere(sphereBounds, lines);
		//	}
		//}
	//}

	void GD_VulkanRenderableMesh::Draw()
	{		
		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		auto vulkanMesh = std::dynamic_pointer_cast<GD_VulkanStaticMesh>(_mesh);
		auto meshPSO = _state;

		auto gpuVertexBuffer = vulkanMesh->GetVertexBuffer()->GetGPUBuffer();
		auto gpuIndexBuffer = vulkanMesh->GetIndexBuffer()->GetGPUBuffer();

		auto &vulkVB = gpuVertexBuffer->GetAs<VulkanBuffer>();
		auto &vulkIB = gpuIndexBuffer->GetAs<VulkanBuffer>();

		auto parentScene = (VulkanRenderScene*)_parentScene;
		auto cameraBuffer = parentScene->GetCameraBuffer();
		auto drawConstBuffer = parentScene->GetCameraBuffer();

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vulkVB.GetBuffer(), offsets);
		vkCmdBindIndexBuffer(commandBuffer, vulkIB.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

		auto CurPool = GGlobalVulkanGI->GetActiveDescriptorPool();

		auto& descriptorSetLayouts = meshPSO->GetDescriptorSetLayouts();
		auto setStartIdx = meshPSO->GetStartIdx();

		std::vector<VkDescriptorSet> locaDrawSets;
		locaDrawSets.resize(descriptorSetLayouts.size());

		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(CurPool, descriptorSetLayouts.data(), descriptorSetLayouts.size());
		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice, &allocInfo, locaDrawSets.data()));

		//set 0
		{
			VkDescriptorBufferInfo perFrameInfo;
			perFrameInfo.buffer = cameraBuffer->GetBuffer();
			perFrameInfo.offset = 0;
			perFrameInfo.range = cameraBuffer->GetPerElementSize();

			VkDescriptorBufferInfo drawConstsInfo;
			drawConstsInfo.buffer = _drawConstantsBuffer->GetBuffer();
			drawConstsInfo.offset = 0;
			drawConstsInfo.range = _drawConstantsBuffer->GetPerElementSize();

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(locaDrawSets[0],
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0, &perFrameInfo),
				vks::initializers::writeDescriptorSet(locaDrawSets[0],
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, &drawConstsInfo),
			};

			vkUpdateDescriptorSets(vulkanDevice,
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);
		}
		//set 1
		{
			auto& textures = _material->GetTextureArray();
			auto gpuTexture = textures.empty() ? GGlobalVulkanGI->GetDefaultTexture() : textures[0]->GetGPUTexture();
			auto &currentVulkanTexture = gpuTexture->GetAs<VulkanTexture>();

			VkDescriptorImageInfo textureInfo = currentVulkanTexture.GetDescriptor();

			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(locaDrawSets[1],
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0, &textureInfo),
				vks::initializers::writeDescriptorSet(locaDrawSets[1],
					VK_DESCRIPTOR_TYPE_SAMPLER, 1, &textureInfo),
			};

			vkUpdateDescriptorSets(vulkanDevice,
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);
		}

		uint32_t uniform_offsets[] = {
			(sizeof(GPUViewConstants)) * currentFrame,
			0
		};

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPSO->GetVkPipeline());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
			meshPSO->GetVkPipelineLayout(),
			setStartIdx,
			locaDrawSets.size(), locaDrawSets.data(), ARRAY_SIZE(uniform_offsets), uniform_offsets);
		vkCmdDrawIndexed(commandBuffer, gpuIndexBuffer->GetElementCount(), 1, 0, 0, 0);
	}
}