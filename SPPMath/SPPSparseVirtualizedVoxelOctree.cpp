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
  
    struct SVVOLevel
	{
	private:
        size_t _maximumSize = 0;
        size_t _pageSize = 0;
        size_t _pageRatio = 0;

        size_t _dataTypeSize = 0;

        
        Vector3i _dimensions = {0,0,0};
        Vector3i _dimensionsPow2 = {0,0,0};
		uint8_t* _basePtr = nullptr;
        std::vector<bool> _pages;        

        size_t _activePages = 0;                

        Vector3i _vCubeVoxelDimensions = {0,0,0};
        Vector3i _vCubeVoxelDimensionsP2 = {0,0,0};

        Vector3i _vCubeCount = {0,0,0};
        Vector3i _vCubeMask = {0,0,0};

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

            //CONFIRM DesiredPageSize is a multiple of GSystemData.PageSize ? 
            //DesiredPageSize / GSystemData.PageSize
            //SE_ASSERT(GSystemData.PageSize DesiredPageSize / )

            _pageSize = DesiredPageSize ? DesiredPageSize : GSystemData.PageSize;
            _pageRatio = DesiredPageSize / GSystemData.PageSize;

            SPP_LOG(LOG_SVVO, LOG_INFO, "SVVOLevel: max size %u", _maximumSize);
            SPP_LOG(LOG_SVVO, LOG_INFO, " - dim: %d %d %d", InDimensions[0], InDimensions[1], InDimensions[2]);            
            SPP_LOG(LOG_SVVO, LOG_INFO, " - system page size: %d", GSystemData.PageSize);
            SPP_LOG(LOG_SVVO, LOG_INFO, " - desired page size: %d", _pageSize);

            // less than 10 pages
            if (_maximumSize < _pageSize * 10)
            {
                _bVirtualAlloc = false;
                _vCubeVoxelDimensions = InDimensions;
                _vCubeVoxelDimensionsP2 = Vector3i{ powerOf2(_vCubeVoxelDimensions[0]), powerOf2(_vCubeVoxelDimensions[1]), powerOf2(_vCubeVoxelDimensions[2]) };
                _vCubeMask = _vCubeVoxelDimensions - Vector3i{ 1,1,1 };
                _vCubeCount = Vector3i{ 1,1,1 };

                _activePages = 1;
                _pageSize = _maximumSize;

                _basePtr = (uint8_t*)SPP_MALLOC(_maximumSize);
            }
            else
            {
                _bVirtualAlloc = true;

                SPP_LOG(LOG_SVVO, LOG_INFO, " - using virtual alloc");

                // must be power of 2
                SE_ASSERT(_pageSize == roundUpToPow2(_pageSize));
                
                auto spatialDivisor = (int32_t)std::cbrt(_pageSize / DataTypeSize);
                spatialDivisor = roundDownToPow2(spatialDivisor);
                _vCubeVoxelDimensions = Vector3i{ spatialDivisor,  spatialDivisor, spatialDivisor };

                while (_vCubeVoxelDimensions.prod() < _pageSize)
                {                  
                    _vCubeVoxelDimensions[0] <<= 1;
                }

                SE_ASSERT(_vCubeVoxelDimensions.prod() == _pageSize);
                _vCubeVoxelDimensionsP2 = Vector3i{ powerOf2(_vCubeVoxelDimensions[0]), powerOf2(_vCubeVoxelDimensions[1]), powerOf2(_vCubeVoxelDimensions[2]) };
              
                _vCubeMask = _vCubeVoxelDimensions - Vector3i{ 1,1,1 };
                _vCubeCount = _dimensions.array() / _vCubeVoxelDimensions.array();

                SE_ASSERT((_dimensions - Vector3i(_vCubeCount.array() * _vCubeVoxelDimensions.array())).isZero());

                SPP_LOG(LOG_SVVO, LOG_INFO, " - cube voxel dimensions: %d x %d x %d", _vCubeVoxelDimensions[0], _vCubeVoxelDimensions[1], _vCubeVoxelDimensions[2]);
                SPP_LOG(LOG_SVVO, LOG_INFO, " - total cube count: %d x %d x %d", _vCubeCount[0], _vCubeCount[1], _vCubeCount[2]);
                
                auto TotalPages = (size_t)_vCubeCount[0] * (size_t)_vCubeCount[1] * (size_t)_vCubeCount[2];

                SPP_LOG(LOG_SVVO, LOG_INFO, " - page count: %d", TotalPages);
                                
                _pages.resize(TotalPages, false);

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

        struct SizeInfo
        {
            size_t ActivePages;
            size_t AllocatePageSize;
            size_t TotalSize;
        };

        SizeInfo GetSize()
        {
            auto allocatedPageSize = _activePages * _pageSize;
            auto pageArraySize = sizeof(std::vector<bool>) + (sizeof(bool) * _pages.size());
            auto TotalSum = sizeof(SVVOLevel) + pageArraySize + allocatedPageSize;
            return SizeInfo{ _activePages, allocatedPageSize, TotalSum };            
        }

        void ValidatePage(size_t InPage)
        {
            SE_ASSERT(InPage < _pages.size());

            if (!_pages[InPage])
            {
                _activePages++;
#if PLATFORM_WINDOWS
                auto finalAddr = _basePtr + (InPage * _pageSize);
                auto curAddr = VirtualAlloc(
                    finalAddr,
                    _pageSize,
                    MEM_COMMIT,
                    PAGE_READWRITE
                );
                SE_ASSERT(curAddr == finalAddr);
#endif
                _pages[InPage] = true;
            }
        }

        struct PageIdxAndMemOffset
        {
            size_t pageIDX;
            size_t memoffset;
        };

        PageIdxAndMemOffset GetOffsets(const Vector3i& InPosition)
        {           
            // find the local voxel
            Vector3i LocalVoxel = Vector3i{ InPosition[0] & _vCubeMask[0],
                InPosition[1] & _vCubeMask[1],
                InPosition[2] & _vCubeMask[2] };
            auto localVoxelIdx = (LocalVoxel[0] +
                LocalVoxel[1] * _vCubeVoxelDimensions[0] +
                LocalVoxel[2] * _vCubeVoxelDimensions[0] * _vCubeVoxelDimensions[1]);

            if (!_bVirtualAlloc)
            {
                return PageIdxAndMemOffset{
                   0, (size_t)localVoxelIdx* _dataTypeSize
                };
            }

            // find which page we are on
            Vector3i PageCubePos = Vector3i{ InPosition[0] >> _vCubeVoxelDimensionsP2[0],
               InPosition[1] >> _vCubeVoxelDimensionsP2[1],
               InPosition[2] >> _vCubeVoxelDimensionsP2[2] };
            auto pageIdx = (PageCubePos[0] +
                PageCubePos[1] * _vCubeCount[0] +
                PageCubePos[2] * _vCubeCount[0] * _vCubeCount[1]);
                       
            auto memOffset = (pageIdx * _pageSize) + localVoxelIdx * _dataTypeSize;

            return PageIdxAndMemOffset{
                (size_t)pageIdx, (size_t)memOffset
            };
        }

        template<typename T>
        void Inc(const Vector3i& InPosition)
        {
            SE_ASSERT(sizeof(T) == _dataTypeSize);

            auto pageAndMem = GetOffsets(InPosition);

            if (_bVirtualAlloc)
            {
                ValidatePage(pageAndMem.pageIDX);
            }

            T& ourValue = *(T*)(_basePtr + pageAndMem.memoffset);
            ourValue++;
        }

        template<typename T>
        void And(const Vector3i& InPosition, T InValue)
        {
            SE_ASSERT(sizeof(T) == _dataTypeSize);

            auto pageAndMem = GetOffsets(InPosition);

            if (_bVirtualAlloc)
            {
                ValidatePage(pageAndMem.pageIDX);
            }

            T& ourValue = *(T*)(_basePtr + pageAndMem.memoffset);
            ourValue &= InValue;
        }

        template<typename T>
        void Or(const Vector3i& InPosition, T InValue)
        {
            SE_ASSERT(sizeof(T) == _dataTypeSize);

            auto pageAndMem = GetOffsets(InPosition);

            if (_bVirtualAlloc)
            {
                ValidatePage(pageAndMem.pageIDX);
            }

            T& ourValue = *(T*)(_basePtr + pageAndMem.memoffset);
            ourValue |= InValue;
        }

        template<typename T>
        void Dec(const Vector3i& InPosition)
        {
            SE_ASSERT(sizeof(T) == _dataTypeSize);

            auto pageAndMem = GetOffsets(InPosition);

            if (_bVirtualAlloc)
            {
                ValidatePage(pageAndMem.pageIDX);
            }

            T& ourValue = *(T*)(_basePtr + pageAndMem.memoffset);
            ourValue--;
        }

		template<typename T>
		bool Set(const Vector3i& InPosition, T InValue)
		{
            SE_ASSERT(InPosition[0] >= 0 && InPosition[1] >= 0 && InPosition[2] >= 0);
            SE_ASSERT(InPosition[0] < _dimensions[0] && InPosition[1] < _dimensions[0] && InPosition[2] < _dimensions[0]);
			SE_ASSERT(sizeof(T) == _dataTypeSize);

			bool bValueChaned = true;
			//bool bIsSet = true;

			auto pageAndMem = GetOffsets(InPosition);

			if (_bVirtualAlloc)
			{
				ValidatePage(pageAndMem.pageIDX);
			}

			T& ourValue = *(T*)(_basePtr + pageAndMem.memoffset);

			if (ourValue == InValue) bValueChaned = false;
			else ourValue = InValue;

			return bValueChaned;
		}

		template<typename T>
		T Get(const Vector3i& InPosition)
        {
            SE_ASSERT(InPosition[0] >= 0 && InPosition[1] >= 0 && InPosition[2] >= 0);
            SE_ASSERT(InPosition[0] < _dimensions[0] && InPosition[1] < _dimensions[0] && InPosition[2] < _dimensions[0]);
            SE_ASSERT(sizeof(T) == _dataTypeSize);

            auto pageAndMem = GetOffsets(InPosition);

            if (_bVirtualAlloc && !_pages[pageAndMem.pageIDX] )
            {
                return 0;             
            }
            
            return *(T*)(_basePtr + pageAndMem.memoffset);
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
        Vector3i VoxelCounts = Vector3i{ roundUpToPow2( (int32_t)std::ceilf(InExtents[0] / VoxelSize) ),
            roundUpToPow2((int32_t)std::ceilf(InExtents[1] / VoxelSize)) ,
            roundUpToPow2((int32_t)std::ceilf(InExtents[2] / VoxelSize)) } * 2;
               
        _dimensions = VoxelCounts;
        _voxelSize = VoxelSize;
        _invVoxelSize = 1.0f / _voxelSize;

        _dimensionsPow2 = Vector3i{ powerOf2(_dimensions[0]),
           powerOf2(_dimensions[1]),
           powerOf2(_dimensions[2])
        };

        Vector3 ActualDimensions = _dimensions.cast<float>() * VoxelSize;
        Vector3 Center = InCenter.cast<float>() * _invVoxelSize + (_dimensions / 2).cast<float>();

        _worldToVoxels = Matrix4x4{
            { _dimensions[0] / ActualDimensions[0], 0,					0,								0 },
            { 0,				 _dimensions[1] / ActualDimensions[1],	0,								0 },
            { 0,				0,					 _dimensions[2] / ActualDimensions[2],					0},
            { Center[0], Center[1], Center[2], 1.0f}
        };       

        //auto maxDimensionPow2 = std::max(_dimensionsPow2[0], std::max(_dimensionsPow2[1], _dimensionsPow2[2]));

        //size_t MaxCombined = std::numeric_limits< uint8_t >::max();
        _levels.clear();
        for (uint32_t Iter = 0; ; Iter++)
        {
            Vector3i CurDimensions = Vector3i{ _dimensions[0] >> Iter,
                _dimensions[1] >> Iter,
                _dimensions[2] >> Iter
            };

            // no longer square chunks
            if (CurDimensions.minCoeff() == 0)
            {
                break;
            }
            auto newLevel = std::make_unique< SVVOLevel >();
            newLevel->Initialize(CurDimensions, 1, DesiredPageSize);            
            //MaxCombined += (MaxCombined << OnesShift);

            _levels.push_back(std::move(newLevel));
        }

        SPP_LOG(LOG_SVVO, LOG_INFO, "SparseVirtualizedVoxelOctree: level count %d", _levels.size());
    }

    SparseVirtualizedVoxelOctree::~SparseVirtualizedVoxelOctree()
    {
        
    }

    void SparseVirtualizedVoxelOctree::SetBox(const Vector3d& InCenter, const Vector3& InExtents, uint8_t InValue)
    {
        Vector3i VoxelCenter = (ToVector4(InCenter.cast<float>()) *  _worldToVoxels).cast<int32_t>().head<3>();;
        Vector3i VoxelExtents = (InExtents * _invVoxelSize).cast<int32_t>();
        Vector3i StartVoxel = VoxelCenter - VoxelExtents;
        Vector3i EndVoxel = VoxelCenter + VoxelExtents;
        for (int32_t IterX = StartVoxel[0]; IterX < EndVoxel[0]; IterX++)
        {
            for (int32_t IterY = StartVoxel[1]; IterY < EndVoxel[1]; IterY++)
            {
                for (int32_t IterZ = StartVoxel[2]; IterZ < EndVoxel[2]; IterZ++)
                {
                    Set(Vector3i{ IterX,IterY,IterZ }, InValue);
                }
            }
        }

        for(auto &curLevel : _levels)
        {
            auto curLevelSize = curLevel->GetSize();

            SPP_LOG(LOG_SVVO, LOG_INFO, "SIZE INFO: %u : %u : %u ", curLevelSize.ActivePages, curLevelSize.AllocatePageSize, curLevelSize.TotalSize);
        }
    }

    void SparseVirtualizedVoxelOctree::SetSphere(const Vector3d& InCenter, float InRadius, uint8_t InValue)
    {
        Vector3i VoxelCenter = (ToVector4(InCenter.cast<float>()) * _worldToVoxels).cast<int32_t>().head<3>();
        Vector3 VoxelCenterf = VoxelCenter.cast<float>();
        float VoxelRadius = (InRadius * _invVoxelSize);
        int32_t VoxelRadiusi = (int32_t)std::ceil(VoxelRadius);
        Vector3i VoxelExtents = Vector3i{ VoxelRadiusi, VoxelRadiusi, VoxelRadiusi };
        Vector3i StartVoxel = VoxelCenter - VoxelExtents;
        Vector3i EndVoxel = VoxelCenter + VoxelExtents;
        for (int32_t IterX = StartVoxel[0]; IterX < EndVoxel[0]; IterX++)
        {
            for (int32_t IterY = StartVoxel[1]; IterY < EndVoxel[1]; IterY++)
            {
                for (int32_t IterZ = StartVoxel[2]; IterZ < EndVoxel[2]; IterZ++)
                {
                    Vector3i curPos = Vector3i{ IterX,IterY,IterZ };
                    Vector3 curPosf = curPos.cast<float>();
                    float curDist = (curPosf - VoxelCenterf).norm();

                    if (curDist < VoxelRadius)
                    {
                        Set(curPos, InValue);
                    }
                }
            }
        }                 

        for (auto& curLevel : _levels)
        {
            auto curLevelSize = curLevel->GetSize();

            SPP_LOG(LOG_SVVO, LOG_INFO, "SIZE INFO: %u : %u : %u ", curLevelSize.ActivePages, curLevelSize.AllocatePageSize, curLevelSize.TotalSize);
        }
    }

    void SparseVirtualizedVoxelOctree::GetSlice(const Vector3d& InPosition, EAxis::Value InAxisIsolate, int32_t CurLevel, int32_t& oX, int32_t& oY, std::vector<Color3> &oSliceData)
    {
        SE_ASSERT(CurLevel >= 0 && CurLevel < _levels.size());
        Vector3i wVoxelCenter = (ToVector4(InPosition.cast<float>()) * _worldToVoxels).cast<int32_t>().head<3>();        
        Vector3i VoxelCenter = Vector3i{ wVoxelCenter[0] >> CurLevel, wVoxelCenter[1] >> CurLevel, wVoxelCenter[2] >> CurLevel };

        oX = std::max(1, _dimensions[0] >> CurLevel);
        oY = std::max(1, _dimensions[2] >> CurLevel);

        //RGB
        oSliceData.resize(oX * oY);

        // Y Isolate
        for (int32_t IterY = 0; IterY < oY; IterY++)
        {
            for (int32_t IterX = 0; IterX < oX; IterX++)
            {
                Vector3i posToGet = { IterX, VoxelCenter[1], IterY };
                uint8_t PixelColor = _levels[CurLevel]->Get<uint8_t>(posToGet);
                if (CurLevel && PixelColor)
                {
                    PixelColor = 200;
                }
                oSliceData[(IterY * oX) + IterX] = Color3{ PixelColor, PixelColor, PixelColor };
            }
        }
    }


    void SparseVirtualizedVoxelOctree::Set(const Vector3i& InPos, uint8_t InValue)
    {
        if (InPos[0] < 0 || InPos[1] < 0 || InPos[2] < 0)
        {
            return;
        }
        if (InPos[0] >= _dimensions[0] || InPos[1] >= _dimensions[1] || InPos[2] >= _dimensions[2])
        {
            return;
        }
        // lowest level is a value change
        if (_levels.front()->Set<uint8_t>(InPos, InValue))
        {
            bool IsOr = (InValue != 0) ? true : false;

            Vector3i CurPos = InPos;

            for (uint32_t Iter = 1; Iter < _levels.size(); Iter++)
            {
                uint8_t SubIdx = (CurPos[0] & 0x01) +
                    ((CurPos[1] & 0x01) << 1) +
                    ((CurPos[2] & 0x01) << 2);
                uint8_t SubIdxMsk = 1 << SubIdx;
                CurPos = Vector3i{ CurPos[0] >> 1,
                    CurPos[1] >> 1,
                    CurPos[2] >> 1
                };

                if(IsOr) _levels[Iter]->Or< uint8_t>(CurPos, SubIdx);
                else _levels[Iter]->And< uint8_t>(CurPos, ~SubIdx);
            }
        }

        
    }

    uint8_t SparseVirtualizedVoxelOctree::Get(const Vector3i& InPos)
    {
        return _levels.front()->Get<uint8_t>(InPos);
    }

    template<typename T, typename O = T>
    O hlslStep(const T& InValueA, const T& InValueB)
    {
        SE_ASSERT(InValueA.size() == InValueB.size());

        O oVal = O::Zero();
        for (int32_t Iter = 0; Iter < InValueA.size(); ++Iter)
        {            
            if (InValueB[Iter] >= InValueA[Iter]) oVal[Iter] = 1;
        }
        return oVal;
    }

    template<typename T, typename O=T>
    O hlslSign(const T& InValue)
    {
        O oSign = O::Zero();
        for (int32_t Iter = 0; Iter < InValue.size(); ++Iter)
        {
            const auto& curValue = InValue[Iter];
            if (curValue > 0) oSign[Iter] = 1;
            if (curValue < 0) oSign[Iter] = -1;
        }
        return oSign;
    }

    template<typename T, typename O = T>
    O hlslFloor(const T& InValue)
    {
        O oVal = O::Zero();
        for (int32_t Iter = 0; Iter < InValue.size(); ++Iter)
        {
            oVal[Iter] = std::floor(InValue[Iter]);
        }
        return oVal;
    }

    template<typename T>
    T hlslAbs(const T& InValue)
    {
        T oVal;// = InValue;
        for (int32_t Iter = 0; Iter < InValue.size(); ++Iter)
        {
            oVal[Iter] = std::abs(InValue[Iter]);
        }
        return oVal;
    }

    template<typename T>
    T hlslFract(const T& InValue)
    {
        T oValue = InValue;
        for (int32_t Iter = 0; Iter < oValue.size(); ++Iter)
        {
            auto& curValue = oValue[Iter];
            curValue = curValue - int32_t(curValue);
        }
        return oValue;
    }

    template<typename T>
    Vector3i hlslNegativeSign(const T& InValue)
    {
        Vector3i oSign = Vector3i::Zero();
        for (int32_t Iter = 0; Iter < InValue.size(); ++Iter)
        {
            const auto& curValue = InValue[Iter];
            if (curValue < 0) oSign[Iter] = 1;
        }
        return oSign;
    }

    // Optimized method 
    bool rayIntersectBounds(const AxisAlignedBoundingBox<Vector3>& InBox,
        const Ray& r,
        Vector3& oTMin,
        Vector3& oTMax)
    {
        Vector3 RayOrigin = r.GetOrigin().cast<float>();
        Vector3 InvRayDirection = r.GetDirection();
        Vector3i RayNegSign = hlslNegativeSign(r.GetDirection());
        // min, max
        Vector3 bounds[2] = { InBox.GetMin(), InBox.GetMax() };

        oTMin[0] = (bounds[RayNegSign[0]].x() - RayOrigin[0]) * InvRayDirection[0];
        oTMax[0] = (bounds[1 - RayNegSign[0]].x() - RayOrigin[0]) * InvRayDirection[0];
        oTMin[1] = (bounds[RayNegSign[1]].y() - RayOrigin[1]) * InvRayDirection[1];
        oTMax[1] = (bounds[1 - RayNegSign[1]].y() - RayOrigin[1]) * InvRayDirection[1];
        if ((oTMin[0] > oTMax[1]) || (oTMin[1] > oTMax[0])) return false;
        if (oTMin[1] > oTMin[0]) oTMin[0] = oTMin[1];
        if (oTMax[1] < oTMax[0]) oTMax[0] = oTMax[1];
        oTMin[2] = (bounds[RayNegSign[2]].z() - RayOrigin[2]) * InvRayDirection[2];
        oTMax[2] = (bounds[1 - RayNegSign[2]].z() - RayOrigin[2]) * InvRayDirection[2];
        if ((oTMin[0] > oTMax[2]) || (oTMin[2] > oTMax[0])) return false;
        if (oTMin[2] > oTMin[0]) oTMin[0] = oTMin[2];
        if (oTMax[2] < oTMax[0]) oTMax[0] = oTMax[2];

        return true;
    }


    void SparseVirtualizedVoxelOctree::CastRay(const Ray& InRay)
    {
        auto& rayDir = InRay.GetDirection();
        auto& rayOrg = InRay.GetOrigin();

        Vector3 rayOrgf = rayOrg.cast<float>();
        Vector3 vRayStart = (ToVector4(rayOrgf) * _worldToVoxels).head<3>();

        AxisAlignedBoundingBox<Vector3> ourBox(Vector3(0, 0, 0), _dimensions.cast<float>());

        Vector3 tMin, tMax;
        rayIntersectBounds(ourBox, InRay, tMin, tMax);

#if 1
        //   const int maxSteps = 100;
        Vector3 voxel = hlslFloor(vRayStart) + Vector3(0.5f, 0.5f, 0.5f);
        Vector3 step = hlslSign(rayDir);
        //voxel = voxel + vec3(rd.x > 0.0, rd.y > 0.0, rd.z > 0.0);
        Vector3 tMax = (voxel - vRayStart).cwiseQuotient(rayDir);
        Vector3 tDelta = hlslAbs(rayDir).cwiseInverse();

        Vector3 samplePoss = voxel;
#else
       
        Vector3 pos = vRayStart;

       // Vector3 tMax = (voxel - ro) / rd;
        //Vector3 tDelta = 1.0 / abs(rd);
        
        Vector3 ri = rayDir.cwiseInverse();
        Vector3 rs = hlslSign(rayDir);
        Vector3 ris = ri.cwiseProduct(rs);
        Vector3 dis = (pos - rayOf + Vector3(0.5f,0.5f,0.5f) + rs.cwiseProduct(Vector3(0.5f,0.5f,0.5f))).cwiseProduct( ri );

#endif

        float hitDistance = 0.0f;
        Vector3 oNormal = { 0,0,0 };
        int32_t maxIter = 1024;
        Vector3 dim = Vector3(0,0,0);
        for (int i = 0; i < maxIter; ++i) 
        {
            if (Get(samplePoss.cast<int32_t>()))
            {
                hitDistance = (tMax - tDelta).dot(dim);
                oNormal = -dim.cwiseProduct( step );
                SPP_LOG(LOG_SVVO, LOG_INFO, "HIT!!!");
                break;
            }

            Vector3 tMaxMins = Vector3(tMax[1], tMax[2], tMax[0]).cwiseMin(Vector3(tMax[2], tMax[0], tMax[1]));
            dim = hlslStep(tMax, tMaxMins);
            tMax += dim.cwiseProduct(tDelta);
            samplePoss += dim.cwiseProduct(step);

            SPP_LOG(LOG_SVVO, LOG_INFO, "RAY: %f %f %f", samplePoss[0], samplePoss[1], samplePoss[2]);
        }

    }

    

#if 0

    [[no discard]] bool rayBoxIntersection(const Ray& ray, const Grid3D& grid, value_type& tMin, value_type& tMax,
        value_type t0, value_type t1) noexcept {
        value_type tYMin, tYMax, tZMin, tZMax;
        const value_type x_inv_dir = 1 / ray.direction().x();
        if (x_inv_dir >= 0) {
            tMin = (grid.minBound().x() - ray.origin().x()) * x_inv_dir;
            tMax = (grid.maxBound().x() - ray.origin().x()) * x_inv_dir;
        }
        else {
            tMin = (grid.maxBound().x() - ray.origin().x()) * x_inv_dir;
            tMax = (grid.minBound().x() - ray.origin().x()) * x_inv_dir;
        }

        const value_type y_inv_dir = 1 / ray.direction().y();
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

        const value_type z_inv_dir = 1 / ray.direction().z();
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
        return (tMin < t1&& tMax > t0);
    }


    float castRay(vec3 eye, vec3 ray, out float dist, out vec3 norm) {
        vec3 pos = floor(eye);
        vec3 ri = 1.0 / ray;
        vec3 rs = sign(ray);
        vec3 ris = ri * rs;
        vec3 dis = (pos - eye + 0.5 + rs * 0.5) * ri;

        vec3 dim = vec3(0.0);
        for (int i = 0; i < maxIter; ++i) {
            if (voxelHit(pos)) {
                dist = dot(dis - ris, dim);
                norm = -dim * rs;
                return 1.0;
            }

            dim = step(dis, dis.yzx);
            dim *= (1.0 - dim.zxy);

            dis += dim * ris;
            pos += dim * rs;
        }

        return 0.0;
    }

    float VoxelTrace(vec3 ro, vec3 rd, out bool hit, out vec3 hitNormal, out vec3 pos, out int material)
    {
        const int maxSteps = 100;
        vec3 voxel = floor(ro) + .501;
        vec3 step = sign(rd);
        //voxel = voxel + vec3(rd.x > 0.0, rd.y > 0.0, rd.z > 0.0);
        vec3 tMax = (voxel - ro) / rd;
        vec3 tDelta = 1.0 / abs(rd);
        vec3 hitVoxel = voxel;
        int mat = 0;

        hit = false;

        float hitT = 0.0;
        for (int i = 0; i < maxSteps; i++)
        {
            if (!hit)
            {
                float d = Scene(voxel, mat);
                if (d <= 0.0 && !hit)
                {
                    hit = true;
                    hitVoxel = voxel;
                    material = mat;
                    break;
                }
                bool c1 = tMax.x < tMax.y;
                bool c2 = tMax.x < tMax.z;
                bool c3 = tMax.y < tMax.z;
                if (c1 && c2)
                {
                    if (!hit)
                    {
                        hitNormal = vec3(-step.x, 0.0, 0.0);
                        hitT = tMax.x;
                    }
                    voxel.x += step.x;
                    tMax.x += tDelta.x;

                }
                else if (c3 && !c1)
                {
                    if (!hit)
                    {
                        hitNormal = vec3(0.0, -step.y, 0.0);
                        hitT = tMax.y;
                    }
                    voxel.y += step.y;
                    tMax.y += tDelta.y;
                }
                else
                {
                    if (!hit)
                    {
                        hitNormal = vec3(0.0, 0.0, -step.z);
                        hitT = tMax.z;
                    }
                    voxel.z += step.z;
                    tMax.z += tDelta.z;
                }
            }
        }
        if (hit && (hitVoxel.x > 27.0 || hitVoxel.x < -27.0 || hitVoxel.z < -27.0 || hitVoxel.z > 27.0))
        {
            hit = false;
            return 1000.0;
        }

        pos = ro + hitT * rd;
        return hitT;
    }

    // Voxel ray casting algorithm from "A Fast Voxel Traversal Algorithm for Ray Tracing" 
// by John Amanatides and Andrew Woo
// http://www.cse.yorku.ca/~amana/research/grid.pdf
    hit intersect(vec3 ro, vec3 rd) {
        //Todo: find out why this is so slow
        vec3 pos = floor(ro);

        vec3 step = sign(rd);
        vec3 tDelta = step / rd;


        float tMaxX, tMaxY, tMaxZ;

        vec3 fr = fract(ro);

        tMaxX = tDelta.x * ((rd.x > 0.0) ? (1.0 - fr.x) : fr.x);
        tMaxY = tDelta.y * ((rd.y > 0.0) ? (1.0 - fr.y) : fr.y);
        tMaxZ = tDelta.z * ((rd.z > 0.0) ? (1.0 - fr.z) : fr.z);

        vec3 norm;
        const int maxTrace = 100;

        for (int i = 0; i < maxTrace; i++) {
            hit h = getVoxel(ivec3(pos));
            if (h.didHit) {
                return hit(true, lighting(norm, pos, rd, h.col));
            }

            if (tMaxX < tMaxY) {
                if (tMaxZ < tMaxX) {
                    tMaxZ += tDelta.z;
                    pos.z += step.z;
                    norm = vec3(0, 0, -step.z);
                }
                else {
                    tMaxX += tDelta.x;
                    pos.x += step.x;
                    norm = vec3(-step.x, 0, 0);
                }
            }
            else {
                if (tMaxZ < tMaxY) {
                    tMaxZ += tDelta.z;
                    pos.z += step.z;
                    norm = vec3(0, 0, -step.z);
                }
                else {
                    tMaxY += tDelta.y;
                    pos.y += step.y;
                    norm = vec3(0, -step.y, 0);
                }
            }
        }

        return hit(false, vec3(0, 0, 0));
    }


    void mainImage(out vec4 fragColor, in vec2 fragCoord)
    {
        vec2 screenPos = (fragCoord.xy / iResolution.xy) * 2.0 - 1.0;
        vec3 cameraDir = vec3(0.0, 0.0, 0.8);
        vec3 cameraPlaneU = vec3(1.0, 0.0, 0.0);
        vec3 cameraPlaneV = vec3(0.0, 1.0, 0.0) * iResolution.y / iResolution.x;
        vec3 rayDir = cameraDir + screenPos.x * cameraPlaneU + screenPos.y * cameraPlaneV;
        vec3 rayPos = vec3(0.0, 2.0 * sin(iTime * 2.7), -12.0);

        rayPos.xz = rotate2d(rayPos.xz, iTime);
        rayDir.xz = rotate2d(rayDir.xz, iTime);

        ivec3 mapPos = ivec3(floor(rayPos + 0.));

        vec3 deltaDist = abs(vec3(length(rayDir)) / rayDir);

        ivec3 rayStep = ivec3(sign(rayDir));

        vec3 sideDist = (sign(rayDir) * (vec3(mapPos) - rayPos) + (sign(rayDir) * 0.5) + 0.5) * deltaDist;

        bvec3 mask;

        for (int i = 0; i < MAX_RAY_STEPS; i++) {
            if (getVoxel(mapPos)) break;
            if (USE_BRANCHLESS_DDA) {
                //Thanks kzy for the suggestion!
                mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));
                /*bvec3 b1 = lessThan(sideDist.xyz, sideDist.yzx);
                bvec3 b2 = lessThanEqual(sideDist.xyz, sideDist.zxy);
                mask.x = b1.x && b2.x;
                mask.y = b1.y && b2.y;
                mask.z = b1.z && b2.z;*/
                //Would've done mask = b1 && b2 but the compiler is making me do it component wise.

                //All components of mask are false except for the corresponding largest component
                //of sideDist, which is the axis along which the ray should be incremented.			

                sideDist += vec3(mask) * deltaDist;
                mapPos += ivec3(vec3(mask)) * rayStep;
            }
            else {
                if (sideDist.x < sideDist.y) {
                    if (sideDist.x < sideDist.z) {
                        sideDist.x += deltaDist.x;
                        mapPos.x += rayStep.x;
                        mask = bvec3(true, false, false);
                    }
                    else {
                        sideDist.z += deltaDist.z;
                        mapPos.z += rayStep.z;
                        mask = bvec3(false, false, true);
                    }
                }
                else {
                    if (sideDist.y < sideDist.z) {
                        sideDist.y += deltaDist.y;
                        mapPos.y += rayStep.y;
                        mask = bvec3(false, true, false);
                    }
                    else {
                        sideDist.z += deltaDist.z;
                        mapPos.z += rayStep.z;
                        mask = bvec3(false, false, true);
                    }
                }
            }
        }

        vec3 color;
        if (mask.x) {
            color = vec3(0.5);
        }
        if (mask.y) {
            color = vec3(1.0);
        }
        if (mask.z) {
            color = vec3(0.75);
        }
        fragColor.rgb = color;
        //fragColor.rgb = vec3(0.1 * noiseDeriv);
    }

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