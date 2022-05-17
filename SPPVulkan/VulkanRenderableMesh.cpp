// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"
#include "VulkanDevice.h"
#include "SPPGraphics.h"
#include "SPPGraphicsO.h"
#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"

namespace SPP
{	
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

	class GD_VulkanRenderableMesh : public GD_RenderableMesh
	{
	protected:
		bool _bIsStatic = false;
		GPUReferencer < VulkanPipelineState > _state;

	public:
		GD_VulkanRenderableMesh(GraphicsDevice* InOwner, bool IsStatic) : GD_RenderableMesh(InOwner), _bIsStatic(IsStatic) {}
		
		virtual bool IsStatic() const {
			return _bIsStatic;
		}

		virtual void _AddToScene(class GD_RenderScene* InScene) override;
		virtual void _RemoveFromScene() override {}

		virtual void Draw() override;
		//virtual void Draw() override;
		//virtual void DrawDebug(std::vector< DebugVertex >& lines) override;
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
			return GetVulkanPipelineState(
				_blendState,
				_rasterizerState,
				_depthState,
				topology,
				layout,
				_vertexShader,
				_pixelShader,
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

	std::shared_ptr< class GD_RenderableMesh > VulkanGraphicsDevice::CreateStaticMesh()
	{
		return std::make_shared< GD_VulkanRenderableMesh>(this, true);
	}

	void GD_VulkanRenderableMesh::_AddToScene(class GD_RenderScene* InScene)
	{
		GD_RenderableMesh::_AddToScene(InScene);

		auto vulkanMat = std::dynamic_pointer_cast<GD_Vulkan_Material>(_material);

		_layout = Vulkan_CreateInputLayout();
		_layout->InitializeLayout(_vertexStreams);

		_state = vulkanMat->GetPipelineState(_topology, _layout);
		_cachedRotationScale = Matrix4x4::Identity();
		_cachedRotationScale.block<3, 3>(0, 0) = GenerateRotationScale();
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
		//auto pd3dDevice = GGraphicsDevice->GetDevice();
		//auto perDrawSratchMem = GGraphicsDevice->GetPerFrameScratchMemory();
		//auto perDrawDescriptorHeap = GGraphicsDevice->GetDynamicDescriptorHeap();
		//auto perDrawSamplerHeap = GGraphicsDevice->GetDynamicSamplerHeap();
		//auto cmdList = GGraphicsDevice->GetCommandList();
		//auto currentFrame = GGraphicsDevice->GetFrameCount();


	}
}