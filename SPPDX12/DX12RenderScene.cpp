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

	// lazy externs
	extern GPUReferencer< GPUShader > DX12_CreateShader(EShaderType InType);
	extern std::shared_ptr< ComputeDispatch> DX_12CreateComputeDispatch(GPUReferencer< GPUShader> InCS);
	extern GPUReferencer< GPUBuffer > DX12_CreateStaticBuffer(GPUBufferType InType, std::shared_ptr< ArrayResource > InCpuData);
	extern GPUReferencer< GPUInputLayout > DX12_CreateInputLayout();
	extern GPUReferencer< GPURenderTarget > DX12_CreateRenderTarget(int32_t Width, int32_t Height, TextureFormat Format);
	extern GPUReferencer< GPUTexture > DX12_CreateTexture(int32_t Width, int32_t Height, TextureFormat Format, std::shared_ptr< ArrayResource > RawData, std::shared_ptr< ImageMeta > InMetaInfo);

	static Vector3d HACKS_CameraPos;

	D3D12RenderScene::D3D12RenderScene()
	{
		_debugVS = DX12_CreateShader(EShaderType::Vertex);
		_debugVS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_vs");

		_debugPS = DX12_CreateShader(EShaderType::Pixel);
		_debugPS->CompileShaderFromFile("shaders/debugSolidColor.hlsl", "main_ps");


		_debugLayout = DX12_CreateInputLayout();
		_debugLayout->InitializeLayout({
				{ "POSITION",  InputLayoutElementType::Float3, offsetof(DebugVertex,position) },
				{ "COLOR",  InputLayoutElementType::Float3, offsetof(DebugVertex,color) }
			});

		_debugPSO = GetD3D12PipelineState(EBlendState::Disabled,
			ERasterizerState::NoCull,
			EDepthState::Enabled,
			EDrawingTopology::LineList,
			_debugLayout,
			_debugVS,
			_debugPS,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr);

		_debugResource = std::make_shared< ArrayResource >();
		_debugResource->InitializeFromType< DebugVertex >(10 * 1024);
		_debugBuffer = DX12_CreateStaticBuffer(GPUBufferType::Vertex, _debugResource);


		//
		_fullscreenVS = DX12_CreateShader(EShaderType::Vertex);
		_fullscreenVS->CompileShaderFromFile("shaders/fullscreenShader.hlsl", "main_vs");

		_fullscreenPS = DX12_CreateShader(EShaderType::Pixel);
		_fullscreenPS->CompileShaderFromFile("shaders/fullscreenShader.hlsl", "main_ps");

		_fullscreenLayout = DX12_CreateInputLayout();
		_fullscreenLayout->InitializeLayout({
				{ "POSITION",  InputLayoutElementType::Float2, offsetof(FullscreenVertex,position) }
			});

		_fullscreenPSO = GetD3D12PipelineState(EBlendState::Disabled,
			ERasterizerState::NoCull,
			EDepthState::Enabled,
			EDrawingTopology::TriangleList,
			_fullscreenLayout,
			_fullscreenVS,
			_fullscreenPS,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr);

	}

	void D3D12RenderScene::DrawDebug()
	{
		if (_lines.empty()) return;

		auto pd3dDevice = GGraphicsDevice->GetDevice();
		auto perDrawSratchMem = GGraphicsDevice->GetPerFrameScratchMemory();
		auto cmdList = GGraphicsDevice->GetCommandList();
		auto currentFrame = GGraphicsDevice->GetFrameCount();

		//_octree.WalkNodes([&](const AABBi& InAABB) -> bool
		//	{
		//		auto minValue = InAABB.GetMin().cast<float>();
		//		auto maxValue = InAABB.GetMax().cast<float>();

		//		Vector3 topPoints[4];
		//		Vector3 bottomPoints[4];

		//		topPoints[0] = Vector3(minValue[0], minValue[1], minValue[2]);
		//		topPoints[1] = Vector3(maxValue[0], minValue[1], minValue[2]);
		//		topPoints[2] = Vector3(maxValue[0], minValue[1], maxValue[2]);
		//		topPoints[3] = Vector3(minValue[0], minValue[1], maxValue[2]);

		//		bottomPoints[0] = Vector3(minValue[0], maxValue[1], minValue[2]);
		//		bottomPoints[1] = Vector3(maxValue[0], maxValue[1], minValue[2]);
		//		bottomPoints[2] = Vector3(maxValue[0], maxValue[1], maxValue[2]);
		//		bottomPoints[3] = Vector3(minValue[0], maxValue[1], maxValue[2]);

		//		for (int32_t Iter = 0; Iter < 4; Iter++)
		//		{
		//			int32_t nextPoint = (Iter + 1) % 4;

		//			_lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
		//			_lines.push_back({ topPoints[nextPoint], Vector3(1,1,1) });

		//			_lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
		//			_lines.push_back({ bottomPoints[nextPoint], Vector3(1,1,1) });

		//			_lines.push_back({ topPoints[Iter], Vector3(1,1,1) });
		//			_lines.push_back({ bottomPoints[Iter], Vector3(1,1,1) });
		//		}

		//		return true;
		//	});

		ID3D12RootSignature* rootSig = nullptr;

		if (_debugVS)
		{
			rootSig = _debugVS->GetAs<D3D12Shader>().GetRootSignature();
		}

		cmdList->SetGraphicsRootSignature(rootSig);

		//table 0, shared all constant, scene stuff 
		{
			cmdList->SetGraphicsRootConstantBufferView(0, GetGPUAddrOfViewConstants());

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

			Matrix4x4 matId = Matrix4x4::Identity();
			Vector3d dummyVec(0, 0, 0);

			// write local to world
			auto HeapAddrs = perDrawSratchMem->GetWritable(sizeof(GPUDrawConstants), currentFrame);
			WriteMem(HeapAddrs, offsetof(GPUDrawConstants, LocalToWorldScaleRotation), matId);
			WriteMem(HeapAddrs, offsetof(GPUDrawConstants, Translation), dummyVec);

			cmdList->SetGraphicsRootConstantBufferView(1, HeapAddrs.gpuAddr);
		}

		cmdList->SetPipelineState(_debugPSO->GetState());
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

		auto perFrameSratchMem = GGraphicsDevice->GetPerFrameScratchMemory();

		auto heapChunk = perFrameSratchMem->GetWritable(_lines.size() * sizeof(DebugVertex), currentFrame);

		memcpy(heapChunk.cpuAddr, _lines.data(), _lines.size() * sizeof(DebugVertex));

		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = heapChunk.gpuAddr;
		vbv.StrideInBytes = sizeof(DebugVertex);
		vbv.SizeInBytes = heapChunk.size;;
		cmdList->IASetVertexBuffers(0, 1, &vbv);
		cmdList->DrawInstanced(_lines.size(), 1, 0, 0);

		_lines.clear();
	}


	void D3D12RenderScene::AddToScene(Renderable* InRenderable)
	{
		RenderScene::AddToScene(InRenderable);

		//HACKS FIX

	/*	auto thisMesh = (D3D12RenderableMesh*)InRenderable;
		if (thisMesh->IsStatic())
		{
			_renderMeshes.push_back(thisMesh);
			_bMeshInstancesDirty = true;
		}	*/
	}

	void D3D12RenderScene::RemoveFromScene(Renderable* InRenderable)
	{
		RenderScene::RemoveFromScene(InRenderable);

		//HACKS FIX

		//auto thisMesh = (D3D12RenderableMesh*)InRenderable;
		//if (thisMesh->IsStatic())
		//{
		//	//_renderMeshes.erase(thisMesh);
		//	_bMeshInstancesDirty = true;
		//}
	}

	void D3D12RenderScene::Draw()
	{
		auto pd3dDevice = GGraphicsDevice->GetDevice();
		auto perDrawSratchMem = GGraphicsDevice->GetPerFrameScratchMemory();
		auto cmdList = GGraphicsDevice->GetCommandList();
		auto currentFrame = GGraphicsDevice->GetFrameCount();

		auto backBufferColor = GGraphicsDevice->GetScreenColor();
		auto backBufferDepth = GGraphicsDevice->GetScreenDepth();
		
		if (_bRenderToBackBuffer)
		{
			backBufferColor->GetAs<D3D12RenderTarget>().TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);

			auto colorDesc = backBufferColor->GetAs<D3D12RenderTarget>().GetCPUDescriptorHandle();
			auto depthDesc = backBufferDepth->GetAs<D3D12RenderTarget>().GetCPUDescriptorHandle();

			cmdList->OMSetRenderTargets(1, &colorDesc, FALSE, &depthDesc);
			cmdList->ClearDepthStencilView(depthDesc, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
			cmdList->ClearRenderTargetView(colorDesc, clearColor, 0, nullptr);
		}
		else
		{
			// Set RTs
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle[5] = { 0 };
			int32_t ActiveCount = 0;
			for (ActiveCount = 0; ActiveCount < ARRAY_SIZE(_activeRTs); ActiveCount++)
			{
				if (!_activeRTs[ActiveCount])break;
				rtvHandle[ActiveCount] = _activeRTs[ActiveCount]->GetAs<D3D12RenderTarget>().GetCPUDescriptorHandle();

				// make them draw
				_activeRTs[ActiveCount]->GetAs<D3D12RenderTarget>().TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET);
			}

			if (_bUseBBWithCustomColor)
			{
				auto depthDesc = backBufferDepth->GetAs<D3D12RenderTarget>().GetCPUDescriptorHandle();
				cmdList->OMSetRenderTargets(ActiveCount, rtvHandle, FALSE, &depthDesc);
				cmdList->ClearDepthStencilView(depthDesc, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			}
			else if(_activeDepth)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE depthDescriptor = _activeDepth->GetAs<D3D12RenderTarget>().GetCPUDescriptorHandle();
				cmdList->OMSetRenderTargets(ActiveCount, rtvHandle, FALSE, &depthDescriptor);
				cmdList->ClearDepthStencilView(depthDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			}
			else
			{
				cmdList->OMSetRenderTargets(ActiveCount, rtvHandle, FALSE, nullptr);
			}

			const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
			for (int32_t Iter = 0; Iter < ARRAY_SIZE(_activeRTs); Iter++)
			{
				if (!_activeRTs[ActiveCount])break;
				cmdList->ClearRenderTargetView(_activeRTs[Iter]->GetAs<D3D12RenderTarget>().GetCPUDescriptorHandle(), clearColor, 0, nullptr);
			}
		}

		// if mesh instances dirty update that structure
		//if (_bMeshInstancesDirty)
		//{

		//}

		//TODO SHARED STRUCTURE FOR CROSS HLSL STRUCTS!!
		// 
		//on GPU
		//struct _ViewConstants
		//{
		//	//all origin centered
		//	float4x4 ViewMatrix;
		//	float4x4 ViewProjectionMatrix;
		//	float4x4 InvViewProjectionMatrix;
		//	//real view position
		//	double3 ViewPosition;
		//};

		_declspec(align(256u))
		struct GPUViewConstants
		{
			//all origin centered
			Matrix4x4 ViewMatrix;
			Matrix4x4 ViewProjectionMatrix;
			Matrix4x4 InvViewProjectionMatrix;
			//real view position
			Vector3d ViewPosition;
			Vector4d FrustumPlanes[6];
			float RecipTanHalfFovy;
		};

		_view.GenerateLeftHandFoVPerspectiveMatrix(45.0f, (float)GGraphicsDevice->GetDeviceWidth() / (float)GGraphicsDevice->GetDeviceHeight());
		_view.BuildCameraMatrices();

		Planed frustumPlanes[6];
		_view.GetFrustumPlanes(frustumPlanes);

		// get first index
		_currentFrameMem = perDrawSratchMem->GetWritable(sizeof(GPUViewConstants), currentFrame);

		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, ViewMatrix), _view.GetCameraMatrix());
		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, ViewProjectionMatrix), _view.GetViewProjMatrix());
		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, InvViewProjectionMatrix), _view.GetInvViewProjMatrix());
		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, ViewPosition), _view.GetCameraPosition());

		// FIXME
		HACKS_CameraPos = _view.GetCameraPosition();

		for (int32_t Iter = 0; Iter < ARRAY_SIZE(frustumPlanes); Iter++)
		{
			WriteMem(_currentFrameMem, offsetof(GPUViewConstants, FrustumPlanes[Iter]), frustumPlanes[Iter].coeffs());
		}

		WriteMem(_currentFrameMem, offsetof(GPUViewConstants, RecipTanHalfFovy), _view.GetRecipTanHalfFovy());

		/*
		_octree.WalkElements(frustumPlanes, [](const IOctreeElement* InElement) -> bool
			{
				((Renderable*)InElement)->Draw();
				return true;
			});
		*/

#if 0
		for (auto renderItem : _renderables)
		{
			renderItem->DrawDebug(_lines);
		}
		DrawDebug();
#endif
		//DrawFullScreen();

		for (auto renderItem : _renderables)
		{
			renderItem->Draw();
		}
	};
}