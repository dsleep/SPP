// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
	
namespace SPP
{
    template<typename T>
    class InternalLinkedList
    {
    protected:
        T* _prev = nullptr;
        T* _next = nullptr;

        static T* _root;

    public:
        InternalLinkedList()
        {
            if (_root)
            {
                _root->_prev = static_cast<T*>(this);
                _next = _root;
            }
            _root = static_cast<T*>(this);
        }

        T* GetNext()
        {
            return _next;
        }

        static T* GetRoot()
        {
            return _root;
        }

        virtual ~InternalLinkedList()
        {
            if (_next)
            {
                _next->_prev = _prev;
            }
            if (_prev)
            {
                _prev->_next = _next;
            }

            if (_root == this)
            {
                SE_ASSERT(_prev == nullptr);
                _root = _next;
            }
        }
    };

    class ReferenceCounted
    {
    protected:
        uint32_t _refCnt = 0;

        virtual void NoMoreReferences()
        {
            delete this;
        }

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
        int _line = -1;
        std::string _file;

        virtual void DestroyObject()
        {
            obj->NoMoreReferences();
            obj = nullptr;
            _line = -1;
            _file.clear();
        }

        void decRef()
        {
            if (obj && obj->decRefCnt() == 0)
            {
                DestroyObject();
            }
        }

    public:
        Referencer() {} 
        Referencer(T* InObj) : obj(InObj)
        {
            if (obj)
            {
                obj->incRefCnt();
            }
        }
        Referencer(int line, const char* file, T* InObj) : _line(line), _file(file), obj(InObj)
        {
            if (obj)
            {
                obj->incRefCnt();
            }
        }

        template<typename K = T>
        Referencer(Referencer<K>&& orig) 
        {
            static_assert(std::is_base_of_v<T, K>, "must be base of");

            std::swap(obj, orig.obj);
            std::swap(_line, orig._line);
            std::swap(_file, orig._file);
        }

        Referencer(Referencer<T>& orig)
        {
            obj = orig.RawGet();
            _line = orig.GetLine();
            _file = orig.GetFile();

            if (obj)
            {
                obj->incRefCnt();
            }
        }

        Referencer(const Referencer<T>& orig)
        {
            obj = orig.RawGet();
            _line = orig.GetLine();
            _file = orig.GetFile();
            if (obj)
            {
                obj->incRefCnt();
            }
        }

        template<typename K = T>
        Referencer(const Referencer<K>& orig)
        {
            static_assert(std::is_base_of_v<T, K> || std::is_base_of_v<K, T>, "must be base of");

            obj = dynamic_cast<T*>( orig.RawGet() );
            _line = orig.GetLine();
            _file = orig.GetFile();
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

        int GetLine() const
        {
            return _line;
        }

        const std::string &GetFile() const
        {
            return _file;
        }

        template<typename K>
        bool operator== (const Referencer<T>& right) const
        {
            return obj == right.obj;
        }

		Referencer<T>& operator= (const Referencer<T>& right)
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
            _line = right._line;
            _file = right._file;
			return *this;
		}

        //template<typename K>
        //Referencer<T>& operator= (const Referencer<K>& right)
        //{
        //    static_assert(std::is_base_of_v<T, K>, "must be base of");

        //    if constexpr (std::is_same_v<T, K>)
        //    {
        //        if (this == &right)
        //        {
        //            return *this;
        //        }
        //        if (right.obj)
        //        {
        //            right.obj->incRefCnt();
        //        }
        //        decRef();
        //        obj = right.obj;
        //        return *this;
        //    }
        //    else
        //    {
        //        *this = reinterpret_cast<const Referencer<T>> (right);
        //    }

        //    return *this;
        //}

        void Reset()
        {
            decRef();
            obj = nullptr;
            _line = -1;
            _file.clear();
        }

        operator bool() const
        {
            return (obj != NULL);
        }

        virtual ~Referencer()
        {
            decRef();
        }
    };
}