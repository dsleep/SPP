// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPSceneRendering.h"

namespace SPP
{
	void Renderable::_AddToScene(class GD_RenderScene* InScene)
	{
		SE_ASSERT(InScene);
		SE_ASSERT(IsOnGPUThread());
		_parentScene = InScene;
		_parentScene->AddToScene(this);
	};
	void Renderable::_RemoveFromScene()
	{
		SE_ASSERT(_parentScene);
		SE_ASSERT(IsOnGPUThread());
		_parentScene->RemoveFromScene(this);
		_parentScene = nullptr;
	};


	void GD_RenderableMesh::_AddToScene(class GD_RenderScene* InScene)
	{
		Renderable::_AddToScene(InScene);

		_vertexBuffer = GGD()->CreateBuffer(GPUBufferType::Vertex);
		_indexBuffer = GGD()->CreateBuffer(GPUBufferType::Index);
		
		_vertexBuffer->Initialize(GPUBufferType::Vertex, _vertexResource);
		_indexBuffer->Initialize(GPUBufferType::Vertex, _indexResource);
	}
}