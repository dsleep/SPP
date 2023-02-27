// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPSpan.h"
#include <vector>

namespace SPP
{    
    class ArrayResource
    {
    protected:
        size_t _perElementSize = 0;
        size_t _elementCount = 0;
        std::vector<uint8_t> _data;

    public:
        ArrayResource() {}
        ArrayResource(size_t InPerElementSize, size_t InElementCount) : _perElementSize(InPerElementSize), _elementCount(InElementCount) {}
        ArrayResource(const ArrayResource &InCopy) : _perElementSize(InCopy._perElementSize), _elementCount(InCopy._elementCount), _data(InCopy._data) {}

        template<typename T>
        TSpan<T> InitializeFromType(size_t Count = 1)
        {
            _perElementSize = sizeof(T);
            _elementCount = Count;
            _data.resize(_elementCount * _perElementSize);
            return TSpan<T>((T*)_data.data(), Count);
        }

        template<typename T>
        void InitializeFromType(const T* InData, size_t Count = 1)
        {
            _perElementSize = sizeof(T);
            _elementCount = Count;
            _data.resize(_elementCount * _perElementSize);
            memcpy(_data.data(), InData, _data.size());
        }

        template<typename T>
        TSpan<T> GetSpan()
        {
            SE_ASSERT(_perElementSize == sizeof(T));
            return TSpan<T>((T*)_data.data(), _elementCount);
        }

        const std::vector<uint8_t>& GetRawByteArray() const
        {
            return _data;
        }
        std::vector<uint8_t>& GetRawByteArray() 
        {
            return _data;
        }
        size_t GetElementCount() const
        {
            return _elementCount;
        }
        size_t GetPerElementSize() const
        {
            return _perElementSize;
        }
        const uint8_t* GetElementData() const
        {
            return _data.data();
        }
        uint8_t* GetElementData() 
        {
            return _data.data();
        }
        size_t GetTotalSize() const
        {
            return _data.size();
        }
    };    

    template<typename T>
    class TArrayResource : public ArrayResource
    {
    public:
        TArrayResource() : ArrayResource() {}
        TArrayResource(int32_t InElementCount) : ArrayResource(sizeof(T), InElementCount) {}
                
        TSpan<T> Initialize(int32_t Count)
        {
            _perElementSize = sizeof(T);
            _elementCount = Count;
            _data.resize(_elementCount * _perElementSize);
            return TSpan<T>((T*)_data.data(), Count);
        }
    };
}