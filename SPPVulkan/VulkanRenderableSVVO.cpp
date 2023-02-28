// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.


#include "SPPVulkan.h"

#include "VulkanRenderableSVVO.h"

#include "VulkanShaders.h"
#include "VulkanRenderScene.h"
#include "VulkanTexture.h"
#include "VulkanPipelineState.h"

#include "SPPGraphics.h"
//#include "SPPGraphicsO.h"
#include "SPPFileSystem.h"
#include "SPPSceneRendering.h"
#include "SPPMesh.h"
#include "SPPLogging.h"

#include "SPPSparseVirtualizedVoxelOctree.h"

namespace SPP
{	
	extern VkDevice GGlobalVulkanDevice;
	extern VulkanGraphicsDevice* GGlobalVulkanGI;	

	


	std::shared_ptr< class RT_RenderableSVVO > VulkanGraphicsDevice::CreateRenderableSVVO()
	{		
		static_assert(offsetof(GPU_VoxelInfo, activeLevels) == 0);
		static_assert(offsetof(GPU_VoxelInfo, pageSize) == 4);
		static_assert(offsetof(GPU_VoxelInfo, worldToVoxel) == 16);
		static_assert(offsetof(GPU_VoxelInfo, voxelToWorld) == 80);
		static_assert(offsetof(GPU_VoxelInfo, dimensions) == 144);
		
		static_assert(offsetof(GPU_LevelInfo, VoxelSize) == 0);
		static_assert(offsetof(GPU_LevelInfo, HalfVoxel) == 240);
		static_assert(offsetof(GPU_LevelInfo, localPageMask) == 480);
		static_assert(offsetof(GPU_LevelInfo, localPageVoxelDimensions) == 720);
		static_assert(offsetof(GPU_LevelInfo, localPageVoxelDimensionsP2) == 960);
		static_assert(offsetof(GPU_LevelInfo, localPageCounts) == 1200);

		return Make_RT_Resource(RT_VulkanRenderableSVVO);
	}

	void RT_VulkanRenderableSVVO::SetupResources(const SparseVirtualizedVoxelOctree& InSVVO)
	{
		auto fullLevelInfos = InSVVO.GetFullLevelInfos();

		_voxelBaseInfoCache.worldToVoxel = InSVVO.GetWorldToVoxels();
		_voxelBaseInfoCache.voxelToWorld = InSVVO.GetVoxelToWorld();
		
		_voxelBaseInfoCache.dimensions = fullLevelInfos.front().dimensions;
		_voxelBaseInfoCache.activeLevels = fullLevelInfos.size();
		_voxelBaseInfoCache.pageSize = fullLevelInfos.front().pageSize;
						
		for (int32_t Iter = 0; Iter < MAX_VOXEL_LEVELS; Iter++)
		{
			_voxelLevelInfoCache.VoxelSize[Iter] = Vector3(1 << Iter, 1 << Iter, 1 << Iter);
			_voxelLevelInfoCache.HalfVoxel[Iter] = (Vector3(1 << Iter, 1 << Iter, 1 << Iter) / 2);

			if (Iter < fullLevelInfos.size())
			{
				_voxelLevelInfoCache.localPageMask[Iter] = fullLevelInfos[Iter].localPageMask;
				_voxelLevelInfoCache.localPageVoxelDimensions[Iter] = fullLevelInfos[Iter].localPageVoxelDimensions;
				_voxelLevelInfoCache.localPageVoxelDimensionsP2[Iter] = fullLevelInfos[Iter].localPageVoxelDimensionsP2;
				_voxelLevelInfoCache.localPageCounts[Iter] = fullLevelInfos[Iter].localPageCounts;
			}
			else
			{
				_voxelLevelInfoCache.localPageMask[Iter] = Vector3i(0, 0, 0);
				_voxelLevelInfoCache.localPageVoxelDimensions[Iter] = Vector3i(0, 0, 0);
				_voxelLevelInfoCache.localPageVoxelDimensionsP2[Iter] = Vector3i(0, 0, 0);
				_voxelLevelInfoCache.localPageCounts[Iter] = Vector3i(0, 0, 0);
			}
		}
	}

	void RT_VulkanRenderableSVVO::_AddToRenderScene(class RT_RenderScene* InScene)
	{
		RT_RenderableSVVO::_AddToRenderScene(InScene);

		auto resVoxelBaseInfo = std::make_shared< ArrayResource >();
		resVoxelBaseInfo->InitializeFromType(&_voxelBaseInfoCache);
		_voxelBaseInfoBuffer = Make_GPU(VulkanBuffer, GPUBufferType::Simple, resVoxelBaseInfo); 

		auto resVoxelLevelInfo = std::make_shared< ArrayResource >();
		resVoxelLevelInfo->InitializeFromType(&_voxelLevelInfoCache);
		_voxelLevelInfoBuffer = Make_GPU(VulkanBuffer, GPUBufferType::Simple, resVoxelLevelInfo);
	}

	void RT_VulkanRenderableSVVO::_RemoveFromRenderScene()
	{
		RT_RenderableSVVO::_RemoveFromRenderScene();

		_voxelBaseInfoBuffer.Reset();
		_voxelLevelInfoBuffer.Reset();
	}

}