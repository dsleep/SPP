// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPSparseVirtualizedVoxelOctree.h"
#include "SPPLogging.h"

#if PLATFORM_WINDOWS
    #include <windows.h>
#endif

namespace SPP
{
    LogEntry LOG_SVVO("SVVO");

    struct SystemData
    {
        size_t PageSize = 0;
    };
    static SystemData GSystemData;
    void SystemInfoInit()
    {
        static bool bDidInit = false;
        if (!bDidInit)
        {
            SYSTEM_INFO info;
            GetSystemInfo(&info);
            GSystemData.PageSize = info.dwPageSize;

            bDidInit = true;
        }
    }
  
    enum class EValueSet
    {
        Set,
        Increment,
        Decrement
    };

    struct SVVOLevel
	{
	private:
        size_t _maximumSize = 0;
        size_t _pageSize = 0;
        size_t _dataTypeSize = 0;
        int32_t _spatialCubeExtent = 0;
        Vector3i _dimensions = {};
        Vector3i _dimensionsPow2 = {};
		uint8_t* _basePtr = nullptr;
        std::vector<bool> _pages;
        std::vector<uint16_t> _pageSum;

        size_t _activePages = 0;
        size_t _spatialDivisor = 0;
        Vector3i _spatialExtents = {};

        bool _bVirtualAlloc = false;

	public:
		SVVOLevel() 
        {
            SystemInfoInit();
        }

        void Initialize(const Vector3i &InDimensions, size_t DataTypeSize, size_t DesiredPageSize)
        {
            // power of 2 dimensions
            SE_ASSERT(InDimensions[0] == roundUpToPow2(InDimensions[0]));
            SE_ASSERT(InDimensions[1] == roundUpToPow2(InDimensions[1]));
            SE_ASSERT(InDimensions[2] == roundUpToPow2(InDimensions[2]));

            _dimensions = InDimensions;
            _dimensionsPow2 = Vector3i{ powerOf2(_dimensions[0]),
               powerOf2(_dimensions[1]),
               powerOf2(_dimensions[2])
            };            
            _dataTypeSize = DataTypeSize;            
            _maximumSize = (size_t)InDimensions[0] * (size_t)InDimensions[1] * (size_t)InDimensions[2] * DataTypeSize;

            _pageSize = DesiredPageSize ? DesiredPageSize : GSystemData.PageSize;

            // less than 10 pages
            if (_maximumSize < _pageSize * 10)
            {
                _bVirtualAlloc = false;
                _basePtr = (uint8_t*)SPP_MALLOC(_maximumSize);
            }
            else
            {
                _bVirtualAlloc = true;
                
                _spatialDivisor = (size_t)std::cbrt(_pageSize / DataTypeSize);
                _spatialCubeExtent = (int32_t)_spatialDivisor;

                _spatialExtents = (InDimensions + Vector3i(_spatialCubeExtent - 1, _spatialCubeExtent - 1, _spatialCubeExtent - 1)) / _spatialCubeExtent;

                auto TotalPages = (size_t)_spatialExtents[0] * (size_t)_spatialExtents[1] * (size_t)_spatialExtents[2];
                                
                _pages.resize(TotalPages, false);
                _pageSum.resize(TotalPages, 0);

                _maximumSize = TotalPages * _pageSize;

#if PLATFORM_WINDOWS
                _basePtr = (uint8_t*)VirtualAlloc(
                    nullptr,
                    _maximumSize,
                    MEM_RESERVE,
                    PAGE_NOACCESS
                );
#endif
            }          

           
            SE_ASSERT(_basePtr);
        }

        size_t ValidatePage(size_t InMemOffset)
        {
            auto currentPage = InMemOffset / GSystemData.PageSize;

            if (!_pages[currentPage])
            {
                _activePages++;
#if PLATFORM_WINDOWS
                auto finalAddr = _basePtr + InMemOffset;
                auto curAddr = VirtualAlloc(
                    finalAddr,
                    _pageSize,
                    MEM_COMMIT,
                    PAGE_READWRITE
                );
                SE_ASSERT(curAddr == finalAddr);
#endif
                _pages[currentPage] = true;
            }

            return currentPage;
        }

        template<typename T>
        bool Setter(const Vector3i& InPosition, T InValue, EValueSet InChange)
        {
            SE_ASSERT(sizeof(T) == _dataTypeSize);

            bool bValueChaned = true;
            bool bIsSet = true;

            Vector3i SpatialCube = InPosition / _spatialCubeExtent;

            //TODO spatial grouping!
            auto memOffset = ( (size_t)InPosition[0] +
                ((size_t)InPosition[1] >> _dimensionsPow2[0]) +
                (((size_t)InPosition[2] >> _dimensionsPow2[0]) >> _dimensionsPow2[1]) ) * _dataTypeSize;

            auto setValue = [&]() {
                T& ourValue = *(T*)(_basePtr + memOffset);
                switch (InChange)
                {
                case EValueSet::Set:
                {
                    if (ourValue == InValue) bValueChaned = false;
                    else ourValue = InValue;
                }
                break;
                case EValueSet::Increment:
                    ourValue += InValue;
                    break;
                case EValueSet::Decrement:
                    ourValue -= InValue;
                    break;
                }
                bIsSet = (ourValue != 0);
            };

            if (_bVirtualAlloc)
            {                
                auto currentPage = ValidatePage(memOffset);

                if (bValueChaned)
                {
                    if (bIsSet) _pageSum[currentPage]++;
                    else _pageSum[currentPage]--;
                }
            }
            // just set it no paging
            else
            {
                setValue();
            }

            return bValueChaned;
        }

        template<typename T>
        T Get(const Vector3i& InPosition)
        {
            SE_ASSERT(sizeof(T) == _dataTypeSize);

            auto memOffset = ((size_t)InPosition[0] +
                ((size_t)InPosition[1] >> _dimensionsPow2[0]) +
                (((size_t)InPosition[2] >> _dimensionsPow2[0]) >> _dimensionsPow2[1])) * _dataTypeSize;

            if (_bVirtualAlloc)
            {                
                auto currentPage = memOffset / GSystemData.PageSize;

                if (!_pages[currentPage])
                {
                    return 0;
                }
            }
            
            return *(T*)(_basePtr + memOffset);            
        }

        SVVOLevel(SVVOLevel const&) = delete;
        SVVOLevel& operator=(SVVOLevel const&) = delete;

        size_t MemUsed()
        {
            return _bVirtualAlloc ? (GSystemData.PageSize * _activePages) : _maximumSize;
        }

        void Free()
        {
            if (_basePtr)
            {
                _pages.clear();
                if (_bVirtualAlloc)
                {
#if PLATFORM_WINDOWS
                    VirtualFree(_basePtr, _maximumSize, MEM_RELEASE);
#endif
                }
                else
                {
                    SPP_FREE(_basePtr);
                }
                _basePtr = nullptr;
            }
        }

        ~SVVOLevel() noexcept
        {
            Free();
        }

        void Move(SVVOLevel&& inMove) noexcept
        {
            std::swap(_maximumSize, inMove._maximumSize);
            std::swap(_basePtr, inMove._basePtr);
            std::swap(_dataTypeSize, inMove._dataTypeSize);
            std::swap(_dimensions, inMove._dimensions);
            std::swap(_bVirtualAlloc, inMove._bVirtualAlloc);
        }

        SVVOLevel(SVVOLevel&& inMove) noexcept
        {
            Move(std::move(inMove));
        }       

        SVVOLevel& operator=(SVVOLevel&& inMove) noexcept
        {
            Free();           
            Move(std::move(inMove));
            return *this;
        }
    };

    SparseVirtualizedVoxelOctree::SparseVirtualizedVoxelOctree(const Vector3d& InCenter, const Vector3& InExtents, float VoxelSize, size_t DesiredPageSize)
    {
        Vector3i Voxels = Vector3i{ roundUpToPow2( (int32_t)std::ceilf(InExtents[0] / VoxelSize) ),
            roundUpToPow2((int32_t)std::ceilf(InExtents[1] / VoxelSize)) ,
            roundUpToPow2((int32_t)std::ceilf(InExtents[2] / VoxelSize)) };
               
        _dimensions = Voxels;
        _dimensionsPow2 = Vector3i{ powerOf2(_dimensions[0]),
           powerOf2(_dimensions[1]),
           powerOf2(_dimensions[2])
        };

        auto maxDimensionPow2 = std::max(_dimensionsPow2[0], std::max(_dimensionsPow2[1], _dimensionsPow2[2]));

        size_t MaxCombined = std::numeric_limits< uint8_t >::max();
        _levels.resize(maxDimensionPow2 + 1);
        for (uint32_t Iter = 0; Iter < _levels.size(); Iter++)
        {
            _levels[Iter] = std::make_unique< SVVOLevel >();

            size_t OnesShift = 0;
            if ((_dimensions[0] >> Iter) != 0) OnesShift++;
            if ((_dimensions[1] >> Iter) != 0) OnesShift++;
            if ((_dimensions[2] >> Iter) != 0) OnesShift++;

            Vector3i CurDimensions = Vector3i{ std::max(1, _dimensions[0] >> Iter),
                std::max(1, _dimensions[1] >> Iter),
                std::max(1, _dimensions[2] >> Iter) 
            };
            
            _levels[Iter]->Initialize(CurDimensions, 1, DesiredPageSize);
            MaxCombined += (MaxCombined << OnesShift);
        }
    }

    SparseVirtualizedVoxelOctree::~SparseVirtualizedVoxelOctree()
    {
        
    }

    void SparseVirtualizedVoxelOctree::Set(const Vector3i& InPos, uint8_t InValue)
    {
        // lowest level is a value change
        if (_levels.front()->Setter<uint8_t>(InPos, InValue, EValueSet::Set))
        {
            EValueSet subSet = (InValue != 0) ? EValueSet::Increment : EValueSet::Decrement;

            for (uint32_t Iter = 1; Iter < _levels.size(); Iter++)
            {
                Vector3i CurPos = Vector3i{ std::max(1, InPos[0] >> Iter),
                    std::max(1, InPos[1] >> Iter),
                    std::max(1, InPos[2] >> Iter)
                };

                _levels[Iter]->Setter<uint8_t>(CurPos, 1, subSet);
            }
        }

        
    }

    uint8_t SparseVirtualizedVoxelOctree::Get(const Vector3i& InPos)
    {
        return _levels.front()->Get<uint8_t>(InPos);
    }


#if 0

    ////////////////////////
    // Macro defined to avoid unnecessary checks with NaNs when using std::max
    #define MAX(a,b) ((a > b ? a : b))

    // Uses the improved version of Smit's algorithm to determine if the given ray will intersect
    // the grid between tMin and tMax. This version causes an additional efficiency penalty,
    // but takes into account the negative zero case.
    // tMin and tMax are then updated to incorporate the new intersection values.
    // Returns true if the ray intersects the grid, and false otherwise.
    // See: http://www.cs.utah.edu/~awilliam/box/box.pdf
    bool rayBoxIntersection(const Ray& ray,
        const Grid3D& grid,
        Vector3& tMin, Vector3& tMax,
        const Vector3& t0,
        const Vector3& t1) noexcept
    {
        Vector3 tYMin, tYMax, tZMin, tZMax;
        const Vector3 x_inv_dir = 1 / ray.direction().x();
        if (x_inv_dir >= 0) {
            tMin = (grid.minBound().x() - ray.origin().x()) * x_inv_dir;
            tMax = (grid.maxBound().x() - ray.origin().x()) * x_inv_dir;
        }
        else {
            tMin = (grid.maxBound().x() - ray.origin().x()) * x_inv_dir;
            tMax = (grid.minBound().x() - ray.origin().x()) * x_inv_dir;
        }

        const Vector3 y_inv_dir = 1 / ray.direction().y();
        if (y_inv_dir >= 0) {
            tYMin = (grid.minBound().y() - ray.origin().y()) * y_inv_dir;
            tYMax = (grid.maxBound().y() - ray.origin().y()) * y_inv_dir;
        }
        else {
            tYMin = (grid.maxBound().y() - ray.origin().y()) * y_inv_dir;
            tYMax = (grid.minBound().y() - ray.origin().y()) * y_inv_dir;
        }

        if (tMin > tYMax || tYMin > tMax) return false;
        if (tYMin > tMin) tMin = tYMin;
        if (tYMax < tMax) tMax = tYMax;

        const Vector3 z_inv_dir = 1 / ray.direction().z();
        if (z_inv_dir >= 0) {
            tZMin = (grid.minBound().z() - ray.origin().z()) * z_inv_dir;
            tZMax = (grid.maxBound().z() - ray.origin().z()) * z_inv_dir;
        }
        else {
            tZMin = (grid.maxBound().z() - ray.origin().z()) * z_inv_dir;
            tZMax = (grid.minBound().z() - ray.origin().z()) * z_inv_dir;
        }

        if (tMin > tZMax || tZMin > tMax) return false;
        if (tZMin > tMin) tMin = tZMin;
        if (tZMax < tMax) tMax = tZMax;
        return (tMin < t1 && tMax > t0);
    }

    void amanatidesWooAlgorithm(const Ray& ray, const Grid3D& grid, const Vector3& t0, const Vector3& t1) noexcept
    {
        Vector3 tMin;
        Vector3 tMax;
        const bool ray_intersects_grid = rayBoxIntersection(ray, grid, tMin, tMax, t0, t1);
        if (!ray_intersects_grid) return;

        tMin = MAX(tMin, t0);
        tMax = MAX(tMax, t1);
        const Vector3 ray_start = ray.origin() + ray.direction() * tMin;
        const Vector3 ray_end = ray.origin() + ray.direction() * tMax;

        size_t current_X_index = MAX(1, std::ceil(ray_start.x() - grid.minBound().x() / grid.voxelSizeX()));
        const size_t end_X_index = MAX(1, std::ceil(ray_end.x() - grid.minBound().x() / grid.voxelSizeX()));
        int stepX;
        Vector3 tDeltaX;
        Vector3 tMaxX;
        if (ray.direction().x() > 0.0) {
            stepX = 1;
            tDeltaX = grid.voxelSizeX() / ray.direction().x();
            tMaxX = tMin + (grid.minBound().x() + current_X_index * grid.voxelSizeX()
                - ray_start.x()) / ray.direction().x();
        }
        else if (ray.direction().x() < 0.0) {
            stepX = -1;
            tDeltaX = grid.voxelSizeX() / -ray.direction().x();
            const size_t previous_X_index = current_X_index - 1;
            tMaxX = tMin + (grid.minBound().x() + previous_X_index * grid.voxelSizeX()
                - ray_start.x()) / ray.direction().x();
        }
        else {
            stepX = 0;
            tDeltaX = tMax;
            tMaxX = tMax;
        }

        size_t current_Y_index = MAX(1, std::ceil(ray_start.y() - grid.minBound().y() / grid.voxelSizeY()));
        const size_t end_Y_index = MAX(1, std::ceil(ray_end.y() - grid.minBound().y() / grid.voxelSizeY()));
        int stepY;
        Vector3 tDeltaY;
        Vector3 tMaxY;
        if (ray.direction().y() > 0.0) {
            stepY = 1;
            tDeltaY = grid.voxelSizeY() / ray.direction().y();
            tMaxY = tMin + (grid.minBound().y() + current_Y_index * grid.voxelSizeY()
                - ray_start.y()) / ray.direction().y();
        }
        else if (ray.direction().y() < 0.0) {
            stepY = -1;
            tDeltaY = grid.voxelSizeY() / -ray.direction().y();
            const size_t previous_Y_index = current_Y_index - 1;
            tMaxY = tMin + (grid.minBound().y() + previous_Y_index * grid.voxelSizeY()
                - ray_start.y()) / ray.direction().y();
        }
        else {
            stepY = 0;
            tDeltaY = tMax;
            tMaxY = tMax;
        }

        size_t current_Z_index = MAX(1, std::ceil(ray_start.z() - grid.minBound().z() / grid.voxelSizeZ()));
        const size_t end_Z_index = MAX(1, std::ceil(ray_end.z() - grid.minBound().z() / grid.voxelSizeZ()));
        int stepZ;
        value_type tDeltaZ;
        value_type tMaxZ;
        if (ray.direction().z() > 0.0) {
            stepZ = 1;
            tDeltaZ = grid.voxelSizeZ() / ray.direction().z();
            tMaxZ = tMin + (grid.minBound().z() + current_Z_index * grid.voxelSizeZ()
                - ray_start.z()) / ray.direction().z();
        }
        else if (ray.direction().z() < 0.0) {
            stepZ = -1;
            tDeltaZ = grid.voxelSizeZ() / -ray.direction().z();
            const size_t previous_Z_index = current_Z_index - 1;
            tMaxZ = tMin + (grid.minBound().z() + previous_Z_index * grid.voxelSizeZ()
                - ray_start.z()) / ray.direction().z();
        }
        else {
            stepZ = 0;
            tDeltaZ = tMax;
            tMaxZ = tMax;
        }

        while (current_X_index != end_X_index || current_Y_index != end_Y_index || current_Z_index != end_Z_index) {
            if (tMaxX < tMaxY && tMaxX < tMaxZ) {
                // X-axis traversal.
                current_X_index += stepX;
                tMaxX += tDeltaX;
            }
            else if (tMaxY < tMaxZ) {
                // Y-axis traversal.
                current_Y_index += stepY;
                tMaxY += tDeltaY;
            }
            else {
                // Z-axis traversal.
                current_Z_index += stepZ;
                tMaxZ += tDeltaZ;
            }
        }
    }
#endif
}