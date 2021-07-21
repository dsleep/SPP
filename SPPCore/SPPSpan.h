// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <vector>

namespace SPP
{
    template<typename T>
    class TSpan
    {
    private:
        size_t _num = 0;
        T* _data = nullptr;

    public:
       

        TSpan() { }
        TSpan(T* InData, size_t InNum) : _data(InData), _num(InNum) {}

        operator T* () const
        {
            return _data;
        }

        T* GetData() const
        {
            return _data;
        }

        T& operator[](std::size_t idx)
        {
            SE_ASSERT(idx < _num);
            return _data[idx];
        }

        size_t GetCount() const
        {
            return _num;
        }

        class iterator
        {
        public:
            iterator(T* ptr) : ptr(ptr) {}
            iterator operator++() { ++ptr; return *this; }
            bool operator!=(const iterator& other) const { return ptr != other.ptr; }
            T& operator*() const { return *ptr; }
        private:
            T* ptr;
        };

        iterator begin() const { return iterator(_data); }
        iterator end() const { return iterator(_data + _num); }
    };
}