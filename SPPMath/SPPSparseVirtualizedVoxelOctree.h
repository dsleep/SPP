// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPMath.h"
#include "SPPPrimitiveShapes.h"
#include <iostream>

namespace SPP
{ 
    class SPP_MATH_API SparseVirtualizedVoxelOctree
    {   
        NO_COPY_ALLOWED(SparseVirtualizedVoxelOctree);
        NO_MOVE_ALLOWED(SparseVirtualizedVoxelOctree);

    private:
        Vector3i _dimensions = {};
        Vector3i _dimensionsPow2 = {};
        float _voxelSize = 0;
        float _invVoxelSize = 0;

        std::vector < std::unique_ptr< struct SVVOLevel > > _levels;
        Matrix4x4 _worldToVoxels;

    public:    
        SparseVirtualizedVoxelOctree(const Vector3d& InCenter, const Vector3& InExtents, float VoxelSize, size_t DesiredPageSize = 0);
        ~SparseVirtualizedVoxelOctree();

        void SetBox(const Vector3d& InCenter, const Vector3& InExtents, uint8_t InValue);
        void SetSphere(const Vector3d& InCenter, float InRadius, uint8_t InValue);

        void Set(const Vector3i &InPos, uint8_t InValue);
        uint8_t Get(const Vector3i& InPos);

        void CastRay(const Ray& InRay);
      
        void GetSlice(const Vector3d& InPosition, EAxis::Value InAxisIsolate, int32_t CurLevel, int32_t &oX, int32_t& oY, std::vector<Color3> &oSliceData );
    };
}