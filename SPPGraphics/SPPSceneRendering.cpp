// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPGraphics.h"
#include "SPPSceneRendering.h"

namespace SPP
{
	void Renderable::AddToScene(class RenderScene* InScene)
	{
		SE_ASSERT(InScene);
		_parentScene = InScene;
		_parentScene->AddToScene(this);
	};
	void Renderable::RemoveFromScene()
	{
		SE_ASSERT(_parentScene);
		_parentScene->RemoveFromScene(this);
	};
}