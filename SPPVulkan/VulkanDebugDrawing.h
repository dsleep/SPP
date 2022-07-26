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

		void AddDebugLine(const Vector3d& Start, const Vector3d& End, const Vector3& Color = Vector3(1, 1, 1));
		void AddDebugBox(const Vector3d& Center, const Vector3d& Extents, const Vector3& Color = Vector3(1,1,1));
		void AddDebugSphere(const Vector3d& Center, float Radius, const Vector3& Color = Vector3(1, 1, 1));

		void PrepareForDraw();
		void Draw();
	};
}