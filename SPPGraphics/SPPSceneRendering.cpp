// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPSceneRendering.h"

namespace SPP
{
	void Renderable::_AddToRenderScene(class GD_RenderScene* InScene)
	{
		SE_ASSERT(InScene);
		SE_ASSERT(IsOnGPUThread());
		_parentScene = InScene;
		_parentScene->AddRenderable(this);
	};
	void Renderable::_RemoveFromRenderScene()
	{
		SE_ASSERT(_parentScene);
		SE_ASSERT(IsOnGPUThread());
		_parentScene->RemoveRenderable(this);
		_parentScene = nullptr;
	};

	GD_RenderableMesh::GD_RenderableMesh(GraphicsDevice* InOwner) : GD_Resource(InOwner)
	{
		_vertexBuffer = _owner->CreateBuffer(GPUBufferType::Vertex);
		_indexBuffer = _owner->CreateBuffer(GPUBufferType::Index);
	}

	void GD_RenderableMesh::_AddToRenderScene(class GD_RenderScene* InScene)
	{
		Renderable::_AddToRenderScene(InScene);

		SE_ASSERT(_indexResource->GetPerElementSize() == 4);
		_vertexBuffer->Initialize(GPUBufferType::Vertex, _vertexResource);
		_indexBuffer->Initialize(GPUBufferType::Index, _indexResource);
	}
}