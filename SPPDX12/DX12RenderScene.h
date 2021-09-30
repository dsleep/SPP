// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "DX12Device.h"
#include "SPPSceneRendering.h"

namespace SPP
{
	class D3D12RenderScene : public RenderScene
	{
	protected:
		D3D12PartialResourceMemory _currentFrameMem;
		//std::vector< D3D12RenderableMesh* > _renderMeshes;
		bool _bMeshInstancesDirty = false;

		GPUReferencer< GPUShader > _debugVS;
		GPUReferencer< GPUShader > _debugPS;
		GPUReferencer< D3D12PipelineState > _debugPSO;
		GPUReferencer< GPUInputLayout > _debugLayout;

		std::shared_ptr < ArrayResource >  _debugResource;
		GPUReferencer< GPUBuffer > _debugBuffer;

		std::vector< DebugVertex > _lines;

		GPUReferencer< GPUShader > _fullscreenVS;
		GPUReferencer< GPUShader > _fullscreenPS;

		GPUReferencer< D3D12PipelineState > _fullscreenPSO;
		GPUReferencer< GPUInputLayout > _fullscreenLayout;


	public:
		D3D12RenderScene();

		GPUReferencer< GPUShader > GetSDFVS()
		{
			return _fullscreenVS;
		}
		GPUReferencer< GPUShader > GetSDFPS()
		{
			return _fullscreenPS;
		}
		GPUReferencer< D3D12PipelineState > GetSDFPSO()
		{
			return _fullscreenPSO;
		}
		GPUReferencer< GPUInputLayout > GetSDFLayout()
		{
			return _fullscreenLayout;
		}

		void DrawDebug();

		virtual void Build() {};

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddrOfViewConstants()
		{
			return _currentFrameMem.gpuAddr;
		}

		virtual void AddToScene(Renderable* InRenderable) override;
		virtual void RemoveFromScene(Renderable* InRenderable) override;
		virtual void Draw() override;
	};
}