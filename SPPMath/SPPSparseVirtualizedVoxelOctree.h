// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPMath.h"
#include "SPPPrimitiveShapes.h"
#include <iostream>

#define MAX_VOXEL_LEVELS 15

namespace SPP
{ 
    struct SVVOLevelInfo
    {
        Vector3i dimensions = { 0, 0, 0 };
        Vector3i dimensionsPow2 = { 0, 0, 0 };
        uint32_t pageSize = 0;

        Vector3i localPageMask = { 0, 0, 0 };
        Vector3i localPageVoxelDimensions = { 0, 0, 0 };
        Vector3i localPageVoxelDimensionsP2 = { 0, 0, 0 };
        Vector3i localPageCounts = { 0, 0, 0 };
    };

    class SPP_MATH_API SparseVirtualizedVoxelOctree
    {   
        NO_COPY_ALLOWED(SparseVirtualizedVoxelOctree);
        NO_MOVE_ALLOWED(SparseVirtualizedVoxelOctree);

    public:

        enum class EPageUpdate
        {
            AddPage,
            RemovePage,
            UpdatePage
        };

        struct VoxelHitInfo
        {
            Vector3 location;
            Vector3 normal;
            uint32_t totalChecks = 0;
        };

    private:
        Vector3i _dimensions = {};
        Vector3i _dimensionsPow2 = {};
        float _voxelSize = 0;
        float _invVoxelSize = 0;
        uint32_t _dirtyCounter = 0;

        uint32_t _pageSize = 0;

        std::vector < std::unique_ptr< struct SVVOLevel > > _levels;
        std::array< std::vector< uint32_t >, MAX_VOXEL_LEVELS > _dirtyPages;

        Matrix4x4 _worldToVoxels;


        struct RayInfo
        {
            Vector3 rayOrg = { 0,0,0 };
            Vector3 rayDir = { 0,0,0 };
            Vector3 rayDirSign = { 0,0,0 };
            Vector3 rayDirInv = { 0,0,0 };
            Vector3 rayDirInvAbs = { 0,0,0 };

            Vector3 lastStep = { 0,0,0 };
            bool bHasStepped = false;
            uint32_t curMisses = 0;
            uint32_t totalTests = 0;
        };      

    public:        

        SparseVirtualizedVoxelOctree(const Vector3d& InCenter, const Vector3& InExtents, float VoxelSize, size_t DesiredPageSize = 0);
        ~SparseVirtualizedVoxelOctree();


        inline bool ValidSample(const Vector3& InPos) const;
        inline uint32_t GetDirtyCounter() const { return _dirtyCounter; }
        inline void DirtyPage(uint8_t InLevel, uint32_t InPage) { _dirtyPages[InLevel].push_back(InPage); }

        // these track changes to report to callback
        void BeginWrite();
        void EndWrite(const std::function<void(uint8_t, uint32_t, const void*)>& InCallback);

        uint32_t GetActivePageCount() const;
        void TouchAllActivePages(const std::function<void(uint8_t, uint32_t, const void*)>& InCallback);

        void SetBox(const Vector3d& InCenter, const Vector3& InExtents, uint8_t InValue);
        void SetSphere(const Vector3d& InCenter, float InRadius, uint8_t InValue);

        void Set(const Vector3i &InPos, uint8_t InValue);
        uint8_t Get(const Vector3i& InPos);
        auto GetLevelCount() const {
            return _levels.size();
        }
        uint32_t GetLevelPageSize(uint8_t InLevel) const;

        uint32_t GetLevelMaxSize(uint8_t InLevel) const;
        bool IsLevelVirtual(uint8_t InLevel) const;
        Vector3i GetDimensions()  const {
            return _dimensions;
        }
        uint8_t GetUnScaledAtLevel(const Vector3i& InPos, uint8_t InLevel);
        auto GetPageSize() const
        {
            return _pageSize;
        }
        
        struct LevelInfo
        {            
            uint32_t PageSize;
            size_t MaxSize;
            bool bIsVirtual;
        };

        std::vector< LevelInfo > GetLevelInfos() const;

        std::vector< SVVOLevelInfo > GetFullLevelInfos() const;

        bool CastRay(const Ray& InRay, VoxelHitInfo &oInfo);      
        void GetSlice(const Vector3d& InPosition, EAxis::Value InAxisIsolate, int32_t CurLevel, int32_t &oX, int32_t& oY, std::vector<Color3> &oSliceData );
    };
}