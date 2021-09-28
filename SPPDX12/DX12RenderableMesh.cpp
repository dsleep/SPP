// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "DX12Device.h"
#include "DX12RenderScene.h"
#include "DX12Shaders.h"
#include "DX12Buffers.h"
#include "DX12Textures.h"

#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"


namespace SPP
{
	extern LogEntry LOG_D3D12Device;

	extern bool ReadyMeshElement(std::shared_ptr<MeshElement> InMeshElement);
	extern bool RegisterMeshElement(std::shared_ptr<MeshElement> InMeshElement);
	extern bool UnregisterMeshElement(std::shared_ptr<MeshElement> InMeshElement);

	// lazy externs
	extern GPUReferencer< GPUShader > DX12_CreateShader(EShaderType InType);
	extern std::shared_ptr< ComputeDispatch> DX_12CreateComputeDispatch(GPUReferencer< GPUShader> InCS);
	extern GPUReferencer< GPUBuffer > DX12_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	extern GPUReferencer< GPUInputLayout > DX12_CreateInputLayout();
	extern GPUReferencer< GPURenderTarget > DX12_CreateRenderTarget(int32_t Width, int32_t Height, TextureFormat Format);
	extern GPUReferencer< GPUTexture > DX12_CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);

	class D3D12RenderableMesh : public RenderableMesh
	{
	protected:
		GPUReferencer<D3D12PipelineState> _state;
		bool _bIsStatic = false;

	public:
		D3D12RenderableMesh(bool IsStatic) : _bIsStatic(IsStatic) {}
		virtual bool IsStatic() const {
			return _bIsStatic;
		}
		virtual void AddToScene(class RenderScene* InScene) override;
		virtual void Draw() override;
		virtual void DrawDebug(std::vector< DebugVertex >& lines) override;
	};

	std::shared_ptr<RenderableMesh> DX12_CreateRenderableMesh(bool bIsStatic)
	{
		return std::make_shared< D3D12RenderableMesh >(bIsStatic);
	}

	class D3D12SDF : public RenderableSignedDistanceField
	{
	protected:		
		GPUReferencer< GPUBuffer > _shapeBuffer;
		std::shared_ptr< ArrayResource > _shapeResource;
		bool _bIsStatic = false;

		GPUReferencer< D3D12PipelineState > _customPSO;

	public:
		D3D12SDF() = default;
		virtual void AddToScene(class RenderScene* InScene) override;
		virtual void Draw() override;
		virtual void DrawDebug(std::vector< DebugVertex >& lines) override;
	};

	std::shared_ptr<D3D12SDF> DX12_CreateSDF()
	{
		return std::make_shared< D3D12SDF >();
	}

	struct D3D12PipelineStateKey
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

		bool operator<(const D3D12PipelineStateKey& compareKey)const
		{
			if (blendState != compareKey.blendState)
			{
				return blendState < compareKey.blendState;
			}
			if (rasterizerState != compareKey.rasterizerState)
			{
				return rasterizerState < compareKey.rasterizerState;
			}
			if (depthState != compareKey.depthState)
			{
				return depthState < compareKey.depthState;
			}
			if (topology != compareKey.topology)
			{
				return topology < compareKey.topology;
			}

			if (inputLayout != compareKey.inputLayout)
			{
				return inputLayout < compareKey.inputLayout;
			}

			if (vs != compareKey.vs)
			{
				return vs < compareKey.vs;
			}
			if (ps != compareKey.ps)
			{
				return ps < compareKey.ps;
			}
			if (ms != compareKey.ms)
			{
				return ms < compareKey.ms;
			}
			if (as != compareKey.as)
			{
				return as < compareKey.as;
			}
			if (hs != compareKey.hs)
			{
				return hs < compareKey.hs;
			}
			if (ds != compareKey.ds)
			{
				return ds < compareKey.ds;
			}
			if (cs != compareKey.cs)
			{
				return cs < compareKey.cs;
			}

			return false;
		}
	};

		
	void D3D12RenderableMesh::AddToScene(class RenderScene* InScene)
	{
		auto _meshData = _meshElements.front();
		
		RenderableMesh::AddToScene(InScene);
		_state = GetD3D12PipelineState(
			_meshData->material->blendState,
			_meshData->material->rasterizerState,
			_meshData->material->depthState,
			_meshData->topology,
			_meshData->material->layout,
			_meshData->material->vertexShader,
			_meshData->material->pixelShader,
			_meshData->material->meshShader,
			_meshData->material->amplificationShader,
			_meshData->material->hullShader,
			_meshData->material->domainShader,
			nullptr);

		_cachedRotationScale = Matrix4x4::Identity();
		_cachedRotationScale.block<3, 3>(0, 0) = GenerateRotationScale();
	}

	template<typename F>
	void NodeTraversal(const Matrix4x4 &InTransform,
		uint32_t CurrentIdx, 
		const std::vector<MeshNode> &MeshletNodes, 
		const Vector3 &InCamPos, 
		uint32_t CurrentLevel,
		const F &func)
	{
		auto& curNode = MeshletNodes[CurrentIdx];

		AABB transformedAABB = curNode.Bounds.Transform(InTransform);
		float Radius = transformedAABB.Extent().norm();
		float DistanceToCamera = std::max( (HACKS_CameraPos.cast<float>() - transformedAABB.Center()).norm() - Radius, 0.0f);


		float DistanceFactor = (DistanceToCamera / 100.0f) * (CurrentLevel+1);

		auto StartIdx = std::get<0>(curNode.ChildrenRange);
		auto EndIdx = std::get<1>(curNode.ChildrenRange);

		auto ChildCount = EndIdx - StartIdx;

		if (DistanceFactor < 10.0f && ChildCount > 0)
		{
			uint32_t StartIdx = std::get<0>(curNode.ChildrenRange);
			uint32_t EndIdx = std::get<1>(curNode.ChildrenRange);

			for (uint32_t IdxIter = StartIdx; IdxIter < EndIdx; IdxIter++)
			{
				NodeTraversal(InTransform, IdxIter, MeshletNodes, InCamPos, CurrentLevel+1, func);
			}
		}
		else
		{			
			func(CurrentIdx);
		}
	}

	void D3D12RenderableMesh::DrawDebug(std::vector< DebugVertex >& lines)
	{
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
	}

	void D3D12RenderableMesh::Draw()
	{		
		auto pd3dDevice = GGraphicsDevice->GetDevice();
		auto perDrawSratchMem = GGraphicsDevice->GetPerDrawScratchMemory();
		auto perDrawDescriptorHeap = GGraphicsDevice->GetDynamicDescriptorHeap();
		auto perDrawSamplerHeap = GGraphicsDevice->GetDynamicSamplerHeap();
		auto cmdList = GGraphicsDevice->GetCommandList();
		auto currentFrame = GGraphicsDevice->GetFrameCount();

		for (auto _meshData : _meshElements)
		{
			ID3D12RootSignature* rootSig = nullptr;

			if (_meshData->material->vertexShader)
			{
				rootSig = _meshData->material->vertexShader->GetAs<D3D12Shader>().GetRootSignature();
			}
			else
			{
				rootSig = _meshData->material->meshShader->GetAs<D3D12Shader>().GetRootSignature();
			}

			cmdList->SetGraphicsRootSignature(rootSig);

			//table 0, shared all constant, scene stuff 
			{
				cmdList->SetGraphicsRootConstantBufferView(0, _parentScene->GetAs<D3D12RenderScene>().GetGPUAddrOfViewConstants());

				CD3DX12_VIEWPORT m_viewport(0.0f, 0.0f, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
				CD3DX12_RECT m_scissorRect(0, 0, GGraphicsDevice->GetDeviceWidth(), GGraphicsDevice->GetDeviceHeight());
				cmdList->RSSetViewports(1, &m_viewport);
				cmdList->RSSetScissorRects(1, &m_scissorRect);
			}

			//table 1, VS only constants
			{
				auto pd3dDevice = GGraphicsDevice->GetDevice();

				_declspec(align(256u))
				struct GPUDrawConstants
				{
					//altered viewposition translated
					Matrix4x4 LocalToWorldScaleRotation;
					Vector3d Translation;
				};

				// write local to world
				auto HeapAddrs = perDrawSratchMem->GetWritable(sizeof(GPUDrawConstants), currentFrame);
				WriteMem(HeapAddrs, offsetof(GPUDrawConstants, LocalToWorldScaleRotation), _cachedRotationScale);
				WriteMem(HeapAddrs, offsetof(GPUDrawConstants, Translation), _position);

				cmdList->SetGraphicsRootConstantBufferView(1, HeapAddrs.gpuAddr);
			}

			//2&3 pixel shaders
			//4 domain
			//5,6 mesh

			//table 4&5 SRV and SAMPLERS
			{
				if (_meshData->material->textureArray.size())
				{
					auto SRVSlotBlock = perDrawDescriptorHeap.GetDescriptorSlots((uint8_t)_meshData->material->textureArray.size());
					auto SamplerSlotBlock = perDrawSamplerHeap.GetDescriptorSlots((uint8_t)_meshData->material->textureArray.size());

					// Describe and create a SRV for the texture.
					for (int32_t Iter = 0; Iter < _meshData->material->textureArray.size(); Iter++)
					{
						SE_ASSERT(_meshData->material->textureArray[Iter]);

						{
							auto psSRVDescriptor = SRVSlotBlock[Iter];

							auto texRef = _meshData->material->textureArray[Iter]->GetAs<D3D12Texture>();

							auto& description = texRef.GetDescription();

							D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
							srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
							srvDesc.Format = description.Format;
							srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
							srvDesc.Texture2D.MipLevels = description.MipLevels;
							pd3dDevice->CreateShaderResourceView(texRef.GetTexture(), &srvDesc, psSRVDescriptor.cpuHandle);
						}

						{
							auto psSamplerDescriptor = SamplerSlotBlock[Iter];

							D3D12_SAMPLER_DESC wrapSamplerDesc = {};
							wrapSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
							wrapSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
							wrapSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
							wrapSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
							wrapSamplerDesc.MinLOD = 0;
							wrapSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
							wrapSamplerDesc.MipLODBias = 0.0f;
							wrapSamplerDesc.MaxAnisotropy = 1;
							wrapSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
							wrapSamplerDesc.BorderColor[0] = wrapSamplerDesc.BorderColor[1] = wrapSamplerDesc.BorderColor[2] = wrapSamplerDesc.BorderColor[3] = 0;

							pd3dDevice->CreateSampler(&wrapSamplerDesc, psSamplerDescriptor.cpuHandle);
						}
					}

					//need to set to CBV, or first one
					cmdList->SetGraphicsRootDescriptorTable(7, SRVSlotBlock.gpuHandle);
					cmdList->SetGraphicsRootDescriptorTable(12, SamplerSlotBlock.gpuHandle);
				}
			}

			cmdList->SetPipelineState(_state->GetState());

			switch (_meshData->topology)
			{
			case EDrawingTopology::PointList:
				cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
				break;
			case EDrawingTopology::LineList:
				cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
				break;
			case EDrawingTopology::TriangleList:
				cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				break;
			case EDrawingTopology::TriangleStrip:
				cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				break;
			case EDrawingTopology::PatchList_4ControlPoints:
				cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
				break;
			default:
				// must have useful topology
				SE_ASSERT(false);
				break;
			}

			cmdList->SetGraphicsRoot32BitConstant(6, _bSelected ? 1 : 0, 0);

			//cmdList->SetComputeRootUnorderedAccessView

			if (_meshData->material->vertexShader)
			{
				cmdList->IASetVertexBuffers(0, 1, _meshData->VertexResource->GetAs<D3D12VertexBuffer>().GetView());

				if (_meshData->IndexResource)
				{
					cmdList->IASetIndexBuffer(_meshData->IndexResource->GetAs<D3D12IndexBuffer>().GetView());
					cmdList->DrawIndexedInstanced(_meshData->IndexResource->GetAs<D3D12IndexBuffer>().GetCachedElementCount(), 1, 0, 0, 0);
				}
				else
				{
					//cmdList->DrawInstanced(static_cast<UINT>(mVertexCount - mBaseVertex), 1, 0, 0);
				}
			}
			else if (_meshData->material->meshShader)
			{
				auto CurType = _meshData->GetType();

				if (CurType == MeshTypes::Meshlets && _meshData->MeshIndex >= 0)
				{
					auto CurMeshElement = (MeshletedElement*)_meshData.get();

					ReadyMeshElement(_meshData);

					cmdList->SetGraphicsRoot32BitConstant(6, _meshData->MeshIndex, 0);
					cmdList->SetGraphicsRoot32BitConstant(6, 4, 1);

#if 0
					auto& StartingNode = CurMeshElement->MeshletNodes.front();


					float DistanceToCamera = 100;// (HACKS_CameraPos.cast<float>() - StartingNode.Bounds.Center()).norm();

					float DistanceFactor = StartingNode.TriCount / DistanceToCamera;

					auto localToWorld = GenerateLocalToWorldMatrix();

					NodeTraversal(localToWorld, 0, CurMeshElement->MeshletNodes, HACKS_CameraPos.cast<float>(), 0, [&](uint32_t IdxIter)
						{
							auto& renderNode = CurMeshElement->MeshletNodes[IdxIter];
							auto meshletCount = std::get<1>(renderNode.MeshletRange) - std::get<0>(renderNode.MeshletRange);
							cmdList->SetGraphicsRoot32BitConstant(6, std::get<0>(renderNode.MeshletRange), 2);
							cmdList->SetGraphicsRoot32BitConstant(6, meshletCount, 3);
							cmdList->SetGraphicsRoot32BitConstant(6, IdxIter, 4);
							cmdList->DispatchMesh(DivRoundUp(meshletCount, 32), 1, 1);
						});

					//if (DistanceFactor > 16)
					//{
					//	uint32_t StartIdx = std::get<0>(_meshData->MeshletNodes[0].ChildrenRange);
					//	uint32_t EndIdx = std::get<1>(_meshData->MeshletNodes[0].ChildrenRange);
					//	for (uint32_t IdxIter = StartIdx; IdxIter < EndIdx; IdxIter++)
					//	{
					//		auto& renderNode = _meshData->MeshletNodes[IdxIter];
					//		auto meshletCount = std::get<1>(renderNode.MeshletRange) - std::get<0>(renderNode.MeshletRange);
					//		cmdList->SetGraphicsRoot32BitConstant(6, std::get<0>(renderNode.MeshletRange), 2);
					//		cmdList->SetGraphicsRoot32BitConstant(6, meshletCount, 3);
					//		cmdList->DispatchMesh(DivRoundUp(meshletCount, 32), 1, 1);
					//	}						
					//}
					//else
					//{
					//	auto& renderNode = _meshData->MeshletNodes[0];
					//	auto meshletCount = std::get<1>(renderNode.MeshletRange) - std::get<0>(renderNode.MeshletRange);
					//	cmdList->SetGraphicsRoot32BitConstant(6, std::get<0>(renderNode.MeshletRange), 2);
					//	cmdList->SetGraphicsRoot32BitConstant(6, meshletCount, 3);
					//	cmdList->DispatchMesh(DivRoundUp(meshletCount, 32), 1, 1);
					//}
#else
					//cmdList->SetGraphicsRoot32BitConstant(6, _meshData->MeshletSubsets.front().Count, 3);

					//for (auto& subset : _meshData->MeshletSubsets)
					//{
					//	cmdList->DispatchMesh(DivRoundUp(subset.Count, 32), 1, 1);
					//}
#endif
				}
			}
		}
	}
}