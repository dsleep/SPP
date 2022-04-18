// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
	
namespace SPP
{
    class ReferenceCounted
    {
        uint32_t _refCnt = 0;

    public:

        uint32_t incRefCnt()
        {
            return ++_refCnt;
        }

        uint32_t decRefCnt()
        {
            SE_ASSERT(_refCnt > 0);
            return --_refCnt;
        }
    };

    template< typename T>
    class Referencer
    {
    protected:
        T* obj = nullptr;

        virtual void DestroyObject()
        {
            delete obj;
            obj = nullptr;
        }

        void decRef()
        {
            if (obj && obj->decRefCnt() == 0)
            {
                DestroyObject();
            }
        }

    public:
        Referencer(T* obj = nullptr) : obj(obj)
        {
            if (obj)
            {
                obj->incRefCnt();
            }
        }

        Referencer(Referencer<T>& orig)
        {
            obj = orig.RawGet();
            if (obj)
            {
                obj->incRefCnt();
            }
        }

        Referencer(const Referencer<T>& orig)
        {
            obj = orig.RawGet();
            if (obj)
            {
                obj->incRefCnt();
            }
        }

        template<typename K = T>
        Referencer(const Referencer<K>& orig)
        {
            static_assert(std::is_base_of_v<T, K>, "must be base of");

            obj = orig.RawGet();
            if (obj)
            {
                obj->incRefCnt();
            }
        }

        T& operator* ()
        {
            SE_ASSERT(obj);
            return *obj;
        }

        T* operator-> ()
        {
            SE_ASSERT(obj);
            return obj;
        }

        T* RawGet() const
        {
            return obj;
        }
        T* get()
        {
            return obj;
        }

        template<typename K>
        bool operator== (const Referencer<T>& right) const
        {
            return obj == right.obj;
        }

        Referencer<T>& operator= (Referencer<T>& right)
        {
            if (this == &right)
            {
                return *this;
            }
            if (right.obj)
            {
                right.obj->incRefCnt();
            }
            decRef();
            obj = right.obj;
            return *this;
        }

        template<typename K>
        Referencer<T>& operator= (Referencer<K>& right)
        {
            static_assert(std::is_base_of_v(T, K));

            *this = reinterpret_cast<Referencer<T>>(right);
        }

        void Reset()
        {
            decRef();
            obj = nullptr;
        }

        operator bool()
        {
            return (obj != NULL);
        }

        virtual ~Referencer()
        {
            decRef();
        }
    };
}