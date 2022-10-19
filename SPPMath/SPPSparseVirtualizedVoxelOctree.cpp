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
        size_t _dataTypeSize = 0;
        Vector3i _dimensions = {};
        Vector3i _dimensionsPow2 = {};
		void* _basePtr = nullptr;
        std::vector<bool> _pages;
        size_t _spatialDivisor = 0;
        size_t _spatialDivisorPow2 = 0;

	public:
		SVVOLevel() 
        {
            SystemInfoInit();
        }

        void Initialize(const Vector3i &InDimensions, size_t DataTypeSize )
        {
            _dimensions = InDimensions;
            _dimensionsPow2 = Vector3i{ powerOf2(_dimensions[0]),
               powerOf2(_dimensions[1]),
               powerOf2(_dimensions[2])
            };
            _dataTypeSize = DataTypeSize;

            _spatialDivisor = (size_t)std::cbrt(GSystemData.PageSize / DataTypeSize);
            _spatialDivisorPow2 = powerOf2(_spatialDivisor);


            _maximumSize = (size_t)InDimensions[0] * (size_t)InDimensions[1] * (size_t)InDimensions[2] * DataTypeSize;
            _maximumSize = RoundUp(_maximumSize, GSystemData.PageSize);

           

            // could be in MB of size for pages
            auto pageCount = _maximumSize / GSystemData.PageSize;
            _pages.resize(pageCount, false);

#if PLATFORM_WINDOWS
            _basePtr = VirtualAlloc(
                nullptr,
                _maximumSize,
                MEM_RESERVE,
                PAGE_NOACCESS
            );
#endif
            SE_ASSERT(_basePtr);
        }

        template<typename T>
        void Set(const Vector3i& InPosition, const T &InValue)
        {
            SE_ASSERT(sizeof(T) == _dataTypeSize);

            //should we spatially group them better?

            auto memOffset = ( (size_t)InPosition[0] +
                (size_t)InPosition[1] >> _dimensionsPow2[0] +
                (size_t)InPosition[2] >> _dimensionsPow2[0] >> _dimensionsPow2[1] ) * _dataTypeSize;

            auto currentPage = memOffset / GSystemData.PageSize;

            if (!_pages[currentPage])
            {
#if PLATFORM_WINDOWS
                 VirtualAlloc(
                    _basePtr + memOffset,
                    GSystemData.PageSize,
                    MEM_COMMIT,
                    PAGE_READWRITE
                );
#endif
                 _pages[currentPage] = true;
            }

            *(T*)(_basePtr + memOffset) = InValue;
        }


        template<typename T>
        T Get(const Vector3i& InPosition)
        {
            SE_ASSERT(sizeof(T) == _dataTypeSize);

            auto memOffset = ((size_t)InPosition[0] +
                ((size_t)InPosition[1] >> _dimensionsPow2[0]) +
                ((size_t)InPosition[2] >> _dimensionsPow2[0]) >> _dimensionsPow2[1]) * _dataTypeSize;

            auto currentPage = memOffset / GSystemData.PageSize;

            if (!_pages[currentPage])
            {
                return 0;
            }
            else
            {
                return *(T*)(_basePtr + memOffset);
            }
        }

        SVVOLevel(SVVOLevel const&) = delete;
        SVVOLevel& operator=(SVVOLevel const&) = delete;

        void Free()
        {
            _pages.clear();
            VirtualFree(_basePtr, _maximumSize, MEM_RELEASE);
        }

        ~SVVOLevel() noexcept
        {
            Free();
        }

        SVVOLevel(SVVOLevel&& inMove)
        {
            std::swap(_maximumSize, inMove._maximumSize);
            std::swap(_basePtr, inMove._basePtr);
            std::swap(_dataTypeSize, inMove._dataTypeSize);
            std::swap(_dimensions, inMove._dimensions);
        }
        SVVOLevel& operator=(SVVOLevel&& inMove)
        {
            Free();           
            SVVOLevel(std::move(inMove));
            return *this;
        }
    };

    void SparseVirtualizedVoxelOctree::Set(const Vector3i& InPos, uint8_t InValue)
    {

    }

    uint8_t SparseVirtualizedVoxelOctree::Get(const Vector3i& InPos)
    {
        return 0;
    }
}