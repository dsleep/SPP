// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPVulkan.h"
#include "VulkanRenderScene.h"

namespace SPP
{
	class PBRDeferredLighting
	{
	protected:
		VulkanGraphicsDevice* _owningDevice = nullptr;
		VulkanRenderScene* _owningScene = nullptr;

	public:
		PBRDeferredLighting(VulkanRenderScene* InScene);

	};
}