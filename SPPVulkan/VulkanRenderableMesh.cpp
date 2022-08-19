// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPVulkan.h"

#include "VulkanRenderableMesh.h"

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

	extern GPUReferencer < VulkanPipelineState >  GetVulkanPipelineState(GraphicsDevice* InOwner, 
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

	GPUReferencer < VulkanPipelineState > RT_Vulkan_Material::GetPipelineState(EDrawingTopology topology,
		GPUReferencer<GPUShader> vsShader,
		GPUReferencer<GPUShader> psShader,
		GPUReferencer<GPUInputLayout> layout)
	{
		SE_ASSERT(vsShader && psShader);

		return GetVulkanPipelineState(_owner,
			_blendState,
			_rasterizerState,
			_depthState,
			topology,
			layout,
			vsShader,
			psShader,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr);
	}

	std::shared_ptr< class RT_Material > VulkanGraphicsDevice::CreateMaterial()
	{
		return Make_RT_Resource(RT_Vulkan_Material, this);
	}

	std::shared_ptr< class RT_StaticMesh > VulkanGraphicsDevice::CreateStaticMesh()
	{
		return Make_RT_Resource( RT_VulkanStaticMesh, this);
	}

	std::shared_ptr< class RT_RenderableMesh > VulkanGraphicsDevice::CreateRenderableMesh()
	{
		return Make_RT_Resource( RT_VulkanRenderableMesh, this);
	}

	void RT_VulkanStaticMesh::Initialize()
	{
		RT_StaticMesh::Initialize();

		_layout = Make_GPU(VulkanInputLayout, _owner); 
		_layout->InitializeLayout(_vertexStreams);		
	}

	void RT_VulkanRenderableMesh::_AddToRenderScene(class RT_RenderScene* InScene)
	{
		RT_RenderableMesh::_AddToRenderScene(InScene);

		_cachedRotationScale = Matrix4x4::Identity();
		_cachedRotationScale.block<3, 3>(0, 0) = GenerateRotationScale();

		if (_bIsStatic)
		{
			_staticDrawLease = GGlobalVulkanGI->GetStaticDrawLease();

			auto curData = _staticDrawLease->Access();
			curData.data.LocalToWorldScaleRotation = _cachedRotationScale;
			curData.data.Translation = _position;
		}
		else
		{
			_drawConstants = std::make_shared< ArrayResource >();
			_drawConstants->InitializeFromType< GPUDrawConstants >(1);
			
			bPendingUpdate = true;
		}
	}

	void RT_VulkanRenderableMesh::_RemoveFromRenderScene()
	{
		RT_RenderableMesh::_RemoveFromRenderScene();
	}

	// MOVE THIS, its a mesh it doesn't have specifics
	void RT_VulkanRenderableMesh::PrepareToDraw()
	{
		if (!_state)
		{
			if (!_material)
			{
				SE_ASSERT(false);
				//_material = _owner->GetDefaultMaterial();
			}

			//auto vulkanMesh = std::dynamic_pointer_cast<RT_VulkanStaticMesh>(_mesh);
			//auto vulkanMat = std::dynamic_pointer_cast<RT_Vulkan_Material>(_material);

			//SE_ASSERT(vulkanMat);
			//SE_ASSERT(_mesh);
			//SE_ASSERT(_mesh->GetLayout());
			//_state = vulkanMat->GetPipelineState(_mesh->GetTopology(), _mesh->GetLayout());

			SE_ASSERT(_state);
		}

		if (!_bIsStatic && bPendingUpdate)
		{
			auto uniformData = _drawConstants->GetSpan< GPUDrawConstants>();
			auto& curData = uniformData[0];
			curData.LocalToWorldScaleRotation = _cachedRotationScale;
			curData.Translation = _position;
			_drawConstantsBuffer = Vulkan_CreateStaticBuffer(_owner, GPUBufferType::Simple, _drawConstants);
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

	void RT_VulkanRenderableMesh::Draw()
	{		
		auto currentFrame = GGlobalVulkanGI->GetActiveFrame();
		auto basicRenderPass = GGlobalVulkanGI->GetBaseRenderPass();
		auto DeviceExtents = GGlobalVulkanGI->GetExtents();
		auto commandBuffer = GGlobalVulkanGI->GetActiveCommandBuffer();
		auto vulkanDevice = GGlobalVulkanGI->GetDevice();
		auto& scratchBuffer = GGlobalVulkanGI->GetPerFrameScratchBuffer();

		auto vulkanMesh = std::dynamic_pointer_cast<RT_VulkanStaticMesh>(_mesh);
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

		auto CurPool = GGlobalVulkanGI->GetPerFrameResetDescriptorPool();

		auto& descriptorSetLayouts = meshPSO->GetDescriptorSetLayouts();
		auto& descriptorSetLayoutBindings = meshPSO->GetDescriptorSetLayoutBindings();
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

			if (_bIsStatic)
			{
				auto staticDrawBuffer = GGlobalVulkanGI->GetStaticInstanceDrawBuffer();
				drawConstsInfo.buffer = staticDrawBuffer->GetBuffer();
				drawConstsInfo.offset = 0;
				drawConstsInfo.range = staticDrawBuffer->GetPerElementSize();
			}
			else
			{
				drawConstsInfo.buffer = _drawConstantsBuffer->GetBuffer();
				drawConstsInfo.offset = 0;
				drawConstsInfo.range = _drawConstantsBuffer->GetPerElementSize();
			}

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
			auto foundFirstSet = descriptorSetLayoutBindings.find(1);

			//FIXME
			//VkDescriptorImageInfo textureInfo[4];
			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			//auto& textures = _material->GetTextureArray();			
			//

			//if (foundFirstSet != descriptorSetLayoutBindings.end())
			//{
			//	int32_t TextureCount = foundFirstSet->second.size() / 2;

			//	for (int32_t Iter = 0; Iter < TextureCount; Iter++)
			//	{
			//		auto gpuTexture = (Iter < textures.size()) ?
			//			textures[Iter]->GetGPUTexture() :
			//			GGlobalVulkanGI->GetDefaultTexture();
			//			
			//		auto& currentVulkanTexture = gpuTexture->GetAs<VulkanTexture>();
			//		textureInfo[Iter] = currentVulkanTexture.GetDescriptor();

			//		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(locaDrawSets[1],
			//			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, (Iter*2) + 0, &textureInfo[Iter]));
			//		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(locaDrawSets[1],
			//			VK_DESCRIPTOR_TYPE_SAMPLER, (Iter * 2) + 1, &textureInfo[Iter]));
			//	}
			//}

			vkUpdateDescriptorSets(vulkanDevice,
				static_cast<uint32_t>(writeDescriptorSets.size()),
				writeDescriptorSets.data(), 0, nullptr);
		}

		uint32_t uniform_offsets[] = {
			(sizeof(GPUViewConstants)) * currentFrame,
			_bIsStatic ? (sizeof(StaticDrawParams) * _staticDrawLease->GetIndex()) : 0
		};

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPSO->GetVkPipeline());
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
			meshPSO->GetVkPipelineLayout(),
			setStartIdx,
			locaDrawSets.size(), locaDrawSets.data(), ARRAY_SIZE(uniform_offsets), uniform_offsets);
		vkCmdDrawIndexed(commandBuffer, gpuIndexBuffer->GetElementCount(), 1, 0, 0, 0);
	}
}