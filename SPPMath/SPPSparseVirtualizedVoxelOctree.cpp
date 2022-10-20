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
        size_t _dataTypeSize = 0;
        Vector3i _dimensions = {};
        Vector3i _dimensionsPow2 = {};
		uint8_t* _basePtr = nullptr;
        std::vector<bool> _pages;
        std::vector<uint16_t> _pageSum;

        size_t _activePages = 0;
        size_t _spatialDivisor = 0;
        size_t _spatialDivisorPow2 = 0;
        bool _bVirtualAlloc = false;

	public:
		SVVOLevel() 
        {
            SystemInfoInit();
        }

        void Initialize(const Vector3i &InDimensions, size_t DataTypeSize )
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

            // less than 10 pages
            if (_maximumSize < GSystemData.PageSize * 10)
            {
                _bVirtualAlloc = false;
                _basePtr = (uint8_t*)SPP_MALLOC(_maximumSize);
            }
            else
            {
                _bVirtualAlloc = true;
                _maximumSize = RoundUp(_maximumSize, GSystemData.PageSize);

                //TODO work up spatial changes
                _spatialDivisor = (size_t)std::cbrt(GSystemData.PageSize / DataTypeSize);
                _spatialDivisorPow2 = powerOf2(_spatialDivisor);

                // could be in MB of size for pages
                auto pageCount = _maximumSize / GSystemData.PageSize;
                _pages.resize(pageCount, false);
                _pageSum.resize(pageCount, 0);

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
                    GSystemData.PageSize,
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

    SparseVirtualizedVoxelOctree::SparseVirtualizedVoxelOctree(const Vector3d& InCenter, const Vector3& InExtents, float VoxelSize)
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
            
            _levels[Iter]->Initialize(CurDimensions, 1);
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
}