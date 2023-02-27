// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPVulkan.h"

#include "SPPGraphics.h"
#include "SPPSceneRendering.h"


#include "VulkanDevice.h"

namespace SPP
{
	struct alignas(16u) GPU_vec3i
	{
		int32_t values[3] = { 0 };

		GPU_vec3i() {} 
		GPU_vec3i& operator =(const Vector3i& InVec)
		{
			values[0] = InVec[0];
			values[1] = InVec[1];
			values[2] = InVec[2];
			return *this;
		}
	};

	struct alignas(16u) GPU_vec3
	{
		float values[3] = { 0 };

		GPU_vec3() {}		
		GPU_vec3 &operator =(const Vector3& InVec)
		{
			values[0] = InVec[0];
			values[1] = InVec[1];
			values[2] = InVec[2];
			return *this;
		}
	};

	struct alignas(16u) GPU_VoxelInfo
	{
		int32_t  activeLevels;
		uint32_t pageSize;
		GPU_vec3i dimensions;
	};

	struct alignas(16u) GPU_LevelInfo 
	{
		GPU_vec3 VoxelSize[MAX_VOXEL_LEVELS];
		GPU_vec3 HalfVoxel[MAX_VOXEL_LEVELS];
		GPU_vec3i localPageMask[MAX_VOXEL_LEVELS];
		GPU_vec3i localPageVoxelDimensions[MAX_VOXEL_LEVELS];
		GPU_vec3i localPageVoxelDimensionsP2[MAX_VOXEL_LEVELS];
		GPU_vec3i localPageCounts[MAX_VOXEL_LEVELS];
	};
	
	class RT_VulkanRenderableSVVO : public RT_RenderableSVVO, public IVulkanPassCacher
	{
		CLASS_RT_RESOURCE();

	protected:				
		GPU_VoxelInfo _voxelBaseInfoCache;
		GPU_LevelInfo _voxelLevelInfoCache;

		GPUReferencer< VulkanBuffer > _voxelBaseInfoBuffer;
		GPUReferencer< VulkanBuffer > _voxelLevelInfoBuffer;

		RT_VulkanRenderableSVVO() {}
	public:

		auto GetVoxelBaseInfo() 
		{
			return _voxelBaseInfoBuffer;
		}
		auto GetVoxelLevelInfo()
		{
			return _voxelLevelInfoBuffer;
		}

		virtual void SetupResources(const SparseVirtualizedVoxelOctree& InSVVO) override;

		virtual ~RT_VulkanRenderableSVVO() {}

		virtual void _AddToRenderScene(class RT_RenderScene* InScene) override;
		virtual void _RemoveFromRenderScene() override;

	};

	

}