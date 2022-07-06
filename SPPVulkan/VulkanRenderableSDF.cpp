// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPVulkan.h"

#include "VulkanDevice.h"
#include "VulkanRenderScene.h"
#include "SPPSDFO.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"


namespace SPP
{
	extern LogEntry LOG_VULKAN;

	// lazy externs
	extern GPUReferencer< VulkanBuffer > Vulkan_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	
	class VulkanSDF : public GD_RenderableSignedDistanceField
	{
	protected:		
		GPUReferencer< GPUBuffer > _shapeBuffer;
		std::shared_ptr< ArrayResource > _shapeResource;
		bool _bIsStatic = false;

		GPUReferencer< PipelineState > _customPSO;

		virtual void _AddToRenderScene(class GD_RenderScene* InScene) override;
	public:
		virtual void Draw() override;
		virtual void DrawDebug(std::vector< DebugVertex >& lines) override;
	};

	std::shared_ptr<GD_RenderableSignedDistanceField> Vulkan_CreateSDF()
	{
		return std::make_shared< VulkanSDF >();
	}
		
	void VulkanSDF::_AddToRenderScene(class GD_RenderScene* InScene)
	{
		GD_RenderableSignedDistanceField::_AddToRenderScene(InScene);

		_cachedRotationScale = Matrix4x4::Identity();
		_cachedRotationScale.block<3, 3>(0, 0) = GenerateRotationScale();

		static_assert((sizeof(SDFShape) * 8) % 128 == 0);

		if (!_shapes.empty())
		{
			_shapeResource = std::make_shared< ArrayResource >();
			auto pShapes = _shapeResource->InitializeFromType<SDFShape>(_shapes.size());
			memcpy(pShapes, _shapes.data(), _shapeResource->GetTotalSize());

			_shapeBuffer = Vulkan_CreateStaticBuffer(GPUBufferType::Array, _shapeResource);


			auto SDFVS = _parentScene->GetAs<VulkanRenderScene>().GetSDFVS();
			auto SDFLayout = _parentScene->GetAs<VulkanRenderScene>().GetRayVSLayout();

			if (_customShader)
			{
				_customPSO = GetVulkanPipelineState(EBlendState::Disabled,
					ERasterizerState::NoCull,
					EDepthState::Enabled,
					EDrawingTopology::TriangleList,
					SDFLayout,
					SDFVS,
					_customShader,
					nullptr,
					nullptr,
					nullptr,
					nullptr,
					nullptr);
			}
		}
	}

	void VulkanSDF::DrawDebug(std::vector< DebugVertex >& lines)
	{
		//for (auto& curShape : _shapes)
		//{
		//	auto CurPos = GetPosition();
		//	DrawSphere(Sphere(curShape.translation + CurPos.cast<float>(), curShape.params[0]), lines);
		//}
	}

	void VulkanSDF::Draw()
	{
		//auto pd3dDevice = GGraphicsDevice->GetDevice();
		//auto perDrawDescriptorHeap = GGraphicsDevice->GetDynamicDescriptorHeap();
		//auto perDrawSratchMem = GGraphicsDevice->GetPerFrameScratchMemory();
		//auto cmdList = GGraphicsDevice->GetCommandList();
		//auto currentFrame = GGraphicsDevice->GetFrameCount();
		//auto curCLWrapper = GGraphicsDevice->GetCommandListWrapper();

		auto SDFVS = _parentScene->GetAs<VulkanRenderScene>().GetSDFVS();
		auto SDFPSO = _parentScene->GetAs<VulkanRenderScene>().GetSDFPSO();
		if (_customPSO)
		{
			SDFPSO = _customPSO;
		}
		//		
		//curCLWrapper->SetRootSignatureFromVerexShader(SDFVS);

		////table 0, shared all constant, scene stuff 
		//{
		//	curCLWrapper->SetupSceneConstants(_parentScene->GetAs<D3D12RenderScene>());
		//}

		////table 1, VS only constants
		//{
		//	auto pd3dDevice = GGraphicsDevice->GetDevice();

		//	_declspec(align(256u))
		//	struct GPUDrawConstants
		//	{
		//		//altered viewposition translated
		//		Matrix4x4 LocalToWorldScaleRotation;
		//		Vector3d Translation;
		//	};

		//	// write local to world
		//	auto HeapAddrs = perDrawSratchMem->GetWritable(sizeof(GPUDrawConstants), currentFrame);
		//	WriteMem(HeapAddrs, offsetof(GPUDrawConstants, LocalToWorldScaleRotation), _cachedRotationScale);
		//	WriteMem(HeapAddrs, offsetof(GPUDrawConstants, Translation), _position);

		//	cmdList->SetGraphicsRootConstantBufferView(1, HeapAddrs.gpuAddr);
		//}

		//curCLWrapper->SetPipelineState(SDFPSO);
		//cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		////3 shapes for now
		//auto ShapeSetBlock = perDrawDescriptorHeap->GetDescriptorSlots(1);

		//SE_ASSERT(_shapeResource->GetElementCount() == _shapes.size());

		//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		//srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		//srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		//srvDesc.Buffer.FirstElement = 0;
		//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		//auto currentTableElement = ShapeSetBlock[0];
		//srvDesc.Buffer.StructureByteStride = _shapeResource->GetPerElementSize(); // We assume we'll only use the first vertex buffer
		//srvDesc.Buffer.NumElements = _shapeResource->GetElementCount();
		//pd3dDevice->CreateShaderResourceView(_shapeBuffer->GetAs<D3D12Buffer>().GetResource(), &srvDesc, currentTableElement.cpuHandle);

		//srvDesc.Buffer.FirstElement = 0;
		//curCLWrapper->AddManualRef(_shapeBuffer);

		//cmdList->SetGraphicsRootDescriptorTable(7, ShapeSetBlock.gpuHandle);
		//cmdList->SetGraphicsRoot32BitConstant(6, _shapeResource->GetElementCount(), 0);
		//cmdList->SetGraphicsRoot32BitConstants(6, 3, _color.data(), 1);

		//cmdList->DrawInstanced(4, 1, 0, 0);
	}

	

}