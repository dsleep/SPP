#pragma once

#include "SPPCore.h"

namespace SPP
{
    template < typename T >
    class CRTPLinkedList
    {
    public:
        CRTPLinkedList()
        {
            Link();
        }
        ~CRTPLinkedList()
        {
            Unlink();
        }

        void Link()
        {
            if (_first())
            {
                _first()->_previous = static_cast<T*>(this);
                _next = _first();
            }

            _first() = static_cast<T*>(this);
        }

        void Unlink()
        {
            if (_first() == this)
            {
                SE_ASSERT(_previous == nullptr);
                _first() = _next;
            }
            else if (_previous)
            {
                // skip it
                _previous->_next = _next;
            }

            if (_next)
            {
                _next->_previous = _previous;
            }

            _previous = nullptr;
            _next = nullptr;
        }

        //void Walk(std::function<void(T*)> iterFunc)
        //{
        //    T* curIter = _first;
        //    while (curIter)
        //    {
        //        iterFunc(curIter);
        //        curIter = curIter->_next;
        //    }
        //}

    private:

        static T*& _first()
        {
            static T* sO = nullptr;
            return sO;
        }
        T* _next = nullptr;
        T* _previous = nullptr;
    };

//    #define IMPLEMEN_CRTP_LL(className) className* SPP::CRTPLinkedList<className>::_first = nullptr;
}