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
        Vector3d _center;
        int32_t _extents = 0;
        uint8_t _extentsP2 = 0;
        uint8_t _maxDepth = 0;
        std::vector < std::unique_ptr< struct SVVOLevel > > _levels;
        Matrix4x4 _worldToOctree;

    public:    
        SparseVirtualizedVoxelOctree(const Vector3d& center, const int32_t Extents, int32_t DesiredLowestExtents, const Matrix4x4& InWorldToOctree = Matrix4x4::Identity())
        {
            _worldToOctree = InWorldToOctree;

            _center = center;
            _extents = roundUpToPow2(Extents);
            _extentsP2 = powerOf2(_extents);
            _maxDepth = powerOf2(_extents / DesiredLowestExtents);
        }
        virtual ~SparseVirtualizedVoxelOctree() = default;

        void Set(const Vector3i &InPos, uint8_t InValue);
        uint8_t Get(const Vector3i& InPos);
    };
}