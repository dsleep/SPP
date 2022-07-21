// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPGPUResources.h"

#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanTools.h"

namespace SPP
{
	class VulkanDebugDrawing
	{
	private:
		struct DataImpl;
		std::unique_ptr<DataImpl> _impl;

		class GraphicsDevice* _owner = nullptr;

	public:
		VulkanDebugDrawing();
		~VulkanDebugDrawing();

		void Initialize(class GraphicsDevice* InOwner);
		void Shutdown();

		void AddDebugLine(const Vector3d& Start, const Vector3d& End, const Color3& Color, float Duration = 1.0f);
		void AddDebugSphere(const Vector3d& Center, float Radius, float Duration = 1.0f);	

		void Draw();
	};
}