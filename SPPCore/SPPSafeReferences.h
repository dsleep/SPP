// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include <stdint.h>
#include <type_traits>

namespace SPP
{
    template< class A, class B >
    inline constexpr bool is_destructible_downcast_v =
        (std::is_base_of_v<B, A> &&
            std::is_polymorphic_v<A> &&
            std::is_polymorphic_v<B> &&
            std::has_virtual_destructor_v<A> &&
            std::has_virtual_destructor_v<B>
            );

    struct SimpleDeleter
    {
        template<class T>
        static void Destroy(T* InValue)
        {
            delete InValue;
        }
    };

    template<class T>
    class SafeControlBlockBase
    {
    protected:
        T* _ptr = nullptr;
        int32_t _weak_count = 0;

        virtual void CheckLife() = 0;

    public:
        SafeControlBlockBase(T* InPtr) : _ptr(InPtr) {}
        virtual ~SafeControlBlockBase() {}

        T* get() const
        {
            return this->_ptr;
        }

        void IncRefWk()
        {
            //SE_ASSERT(_ptr);
            _weak_count++;
        }

        void DecRefWk()
        {
            //SE_ASSERT(_ptr);
            _weak_count--;
            CheckLife();
        }
    };

    template<class T, class Deallocator = void>
    class SafeSharedControlBlock : public SafeControlBlockBase<T>
    {
    protected:
        int32_t _strong_count = 0;

    public:
        SafeSharedControlBlock(T* InPtr) : SafeControlBlockBase<T>(InPtr)
        {

        }

        virtual ~SafeSharedControlBlock() {}

        template< typename = std::enable_if_t< !std::is_same_v<Deallocator, void> > >
        void IncRefStr()
        {
            SE_ASSERT(this->_ptr);
            this->_strong_count++;
        }

        template< typename = std::enable_if_t< !std::is_same_v<Deallocator, void> > >
        void DecRefStr()
        {
            _strong_count--;

            if (_strong_count == 0)
            {
                Deallocator::Destroy(this->_ptr);
                this->_ptr = nullptr;
            }

            CheckLife();
        }

        virtual void CheckLife() override
        {
            SE_ASSERT(this->_weak_count >= 0);
            SE_ASSERT(this->_strong_count >= 0);

            if (this->_strong_count == 0 && this->_weak_count == 0)
            {
                delete this;
            }
        }
    };

    template<class T, class Deallocator = void>
    class SafeOwnedControlBlock : public SafeControlBlockBase<T>
    {
    protected:
        int32_t _lock_count = 0;

    public:
        SafeOwnedControlBlock(T* InPtr) : SafeControlBlockBase<T>(InPtr) {}
        virtual ~SafeOwnedControlBlock() {}

        template< typename = std::enable_if_t< !std::is_same_v<Deallocator, void> > >
        void Destroy()
        {
            SE_ASSERT(this->_lock_count == 0);
            SE_ASSERT(this->_ptr);

            Deallocator::Destroy(this->_ptr);
            this->_ptr = nullptr;

            CheckLife();
        }

        void IncRefLck()
        {
            SE_ASSERT(this->_ptr);
            this->_lock_count++;
        }

        void DecRefLck()
        {
            _lock_count--;
            CheckLife();
        }

        virtual void CheckLife() override
        {
            SE_ASSERT(this->_weak_count >= 0);
            SE_ASSERT(this->_lock_count >= 0);

            if (this->_ptr == nullptr &&
                this->_lock_count == 0 &&
                this->_weak_count == 0)
            {
                delete this;
            }
        }
    };


    template< class T, class LockType  >
    class safe_obj_weak;


    template<class SafeType>
    class enable_weak_obj_from_this
    {
    private:
        template<class T, class Deallocator>
        friend class safe_obj_owned;
        template<class T, class Deallocator>
        friend class safe_obj;

        using WeakType = SafeType::WeakType;

        mutable WeakType _weak_obj_ptr;

    public:
        auto weak_obj_from_this() const noexcept { return _weak_obj_ptr; }

    protected:
        constexpr enable_weak_obj_from_this() noexcept : _weak_obj_ptr() {}
        enable_weak_obj_from_this(const enable_weak_obj_from_this&) noexcept : _weak_obj_ptr() {}
        enable_weak_obj_from_this& operator=(const enable_weak_obj_from_this&) noexcept
        {
            return *this;
        }
        ~enable_weak_obj_from_this() = default;
    };

    struct s_safe_obj {};

    template<class T, class Deallocator = SimpleDeleter>
    class safe_obj
    {
    public:
        using ControlBlockType = SafeSharedControlBlock<T, Deallocator>;
        using WeakType = safe_obj_weak<T, safe_obj >;
        using AsStruct = s_safe_obj;
        using OurType = safe_obj;
        using WeakObjEnableType = enable_weak_obj_from_this<OurType>;

    private:
        ControlBlockType* _ctrl = nullptr;

    public:
        // default constructor
        safe_obj() {}

        auto _getControlBlock() const
        {
            return _ctrl;
        }

        void reset()
        {
            if (_ctrl)
            {
                _ctrl->DecRefStr();
                _ctrl = nullptr;
            }
        }

        T* release()
        {
            T* oT = nullptr;
            if (_ctrl)
            {
                oT = _ctrl->get();
                _ctrl = nullptr;
            }
            return oT;
        }

        template <class U,
            typename = std::enable_if_t< std::is_convertible_v<U*, T*> > >
        void reset(U* InObj)
        {
            reset();

            if (InObj)
            {
                this->_ctrl = new ControlBlockType(InObj);
                this->_ctrl->IncRefStr();

                if constexpr (std::is_base_of_v< WeakObjEnableType, U >)
                {
                    InObj->_weak_obj_ptr = *this;
                }
            }
        }

        template <class U,
            typename = std::enable_if_t< std::is_convertible_v<U*, T*> > >
        explicit safe_obj(U* InObj)
        {
            reset(InObj);
        }

        safe_obj(ControlBlockType* InCtrl)
        {
            _ctrl = InCtrl;
            _ctrl->IncRefStr();
        }

        // copy constructor
        safe_obj(const safe_obj& obj)
        {
            this->_ctrl = obj._ctrl;
            if (this->_ctrl)
            {
                this->_ctrl->IncRefStr();
            }
        }

        template <class U,
            typename = std::enable_if_t< std::is_convertible_v<U*, T*> > >
        safe_obj(const safe_obj<U, Deallocator>& obj)
        {
            this->_ctrl = (ControlBlockType*)obj._getControlBlock();
            if (this->_ctrl)
            {
                this->_ctrl->IncRefStr();
            }
        }

        safe_obj& operator=(const safe_obj& obj)
        {
            reset();

            this->_ctrl = obj._ctrl;
            if (this->_ctrl)
            {
                this->_ctrl->IncRefStr();
            }

            return *this;
        }

        // copy assignment
        template <class U,
            typename = std::enable_if_t< std::is_convertible_v<U*, T*> >
        >
        safe_obj& operator=(const safe_obj<U, Deallocator>& obj)
        {
            reset();

            this->_ctrl = (ControlBlockType*)obj._getControlBlock();
            if (this->_ctrl)
            {
                this->_ctrl->IncRefStr();
            }

            return *this;
        }

        // move constructor
        template <class U,
            typename = std::enable_if_t< std::is_convertible_v<U*, T*> > >
        safe_obj(safe_obj<U, Deallocator>&& InMovObj)
        {
            // same exact refs just do a move
            std::swap(this->_ctrl, InMovObj._ctrl);
        }

        // move assignment
        template <class U,
            typename = std::enable_if_t<
            std::is_convertible_v<U*, T*> || is_destructible_downcast_v<T, U>
        > >
        safe_obj& operator=(safe_obj<U, Deallocator>&& InMovObj)
        {
            reset();

            this->_ctrl = (ControlBlockType*)InMovObj._getControlBlock();
            InMovObj.release();

            return *this;
        }

        ~safe_obj()
        {
            reset();
        }

        //access
        T* operator->() const
        {
            return this->_ctrl->get();
        }
        T& operator*() const
        {
            return *this->_ctrl->get();
        }
        T* get() const
        {
            return this->_ctrl->get();
        }

        operator bool() const
        {
            return (_ctrl != NULL);
        }
    };

    struct s_safe_obj_owned {};
    struct s_safe_obj_locked {};

    template<class T>
    class safe_obj_lock;



    /// <summary>
    /// 
    /// </summary>
    /// <typeparam name="T"></typeparam>
    /// <typeparam name="Deallocator"></typeparam>
    template<class T, class Deallocator = SimpleDeleter>
    class safe_obj_owned
    {
    public:
        using ControlBlockType = SafeOwnedControlBlock<T, Deallocator>;
        using WeakType = safe_obj_weak<T, safe_obj_lock<T> >;
        using AsStruct = s_safe_obj_locked;
        using OurType = safe_obj_owned;
        using WeakObjEnableType = enable_weak_obj_from_this<OurType>;

    private:
        ControlBlockType* _ctrl = nullptr;

    public:
        // default constructor
        safe_obj_owned()
        {
        }

        auto _getControlBlock() const
        {
            return _ctrl;
        }

        // when it loses the ref it dies
        void reset()
        {
            if (_ctrl)
            {
                _ctrl->Destroy();
                _ctrl = nullptr;
            }
        }

        T* release()
        {
            T* oT = nullptr;
            if (_ctrl)
            {
                oT = _ctrl->get();
                _ctrl = nullptr;
            }
            return oT;
        }

        template <class U,
            typename = std::enable_if_t< std::is_convertible_v<U*, T*> > >
        void reset(U* InObj)
        {
            reset();

            if (InObj)
            {
                _ctrl = new ControlBlockType(InObj);

                if constexpr (std::is_base_of_v< WeakObjEnableType, U >)
                {
                    auto objWeakObjShare = reinterpret_cast<WeakObjEnableType*> (InObj);
                    objWeakObjShare->_weak_obj_ptr = *this;
                }
            }
        }

        template <class U,
            typename = std::enable_if_t< std::is_convertible_v<U*, T*> > >
        explicit safe_obj_owned(U* InObj)
        {
            reset(InObj);
        }

        // NO COPIES
        safe_obj_owned(safe_obj_owned const&) = delete;
        safe_obj_owned& operator=(safe_obj_owned const&) = delete;

        // move constructor
        template <class U,
            typename = std::enable_if_t< std::is_convertible_v<U*, T*> > >
        safe_obj_owned(safe_obj_owned<U, Deallocator>&& InMovObj)
        {
            this->_ctrl = (ControlBlockType*)InMovObj._getControlBlock();
            InMovObj.release();
        }

        // move assignment
        template <class U,
            typename = std::enable_if_t<
            std::is_convertible_v<U*, T*> || is_destructible_downcast_v<T, U>

        > >
        safe_obj_owned& operator=(safe_obj_owned<U, Deallocator>&& InMovObj)
        {
            reset();

            this->_ctrl = (ControlBlockType*)InMovObj._getControlBlock();
            InMovObj.release();

            return*this;
        }

        ~safe_obj_owned()
        {
            reset();
        }

        //access
        T* operator->() const
        {
            return this->_ctrl->get();
        }
        T& operator*() const
        {
            return *this->_ctrl->get();
        }
        T* get() const
        {
            return this->_ctrl->get();
        }

        operator bool() const
        {
            return (_ctrl != NULL);
        }
    };

    template<class T>
    class safe_obj_lock
    {
    public:
        using ControlBlockType = SafeOwnedControlBlock<T>;
        using AsStruct = s_safe_obj_locked;

    private:
        ControlBlockType* _ctrl = nullptr;

    public:
        safe_obj_lock() {}
        ~safe_obj_lock()
        {
            reset();
        }

        safe_obj_lock(ControlBlockType* InCtrl)
        {
            _ctrl = InCtrl;
            _ctrl->IncRefLck();
        }

        void reset()
        {
            if (_ctrl)
            {
                _ctrl->DecRefLck();
                _ctrl = nullptr;
            }
        }

        //access
        T* operator->() const
        {
            return this->_ctrl->get();
        }
        T& operator*() const
        {
            return *this->_ctrl->get();
        }
        T* get() const
        {
            return this->_ctrl->get();
        }

        operator bool() const
        {
            return (_ctrl != NULL);
        }
    };

    template< class T, class LockType >
    class safe_obj_weak
    {
    public:
        using ControlBlockType = SafeControlBlockBase<T>;

    private:
        ControlBlockType* _ctrl = nullptr;

    public:
        safe_obj_weak() {}
        ~safe_obj_weak()
        {
            reset();
        }

        auto _getControlBlock() const
        {
            return _ctrl;
        }

        void reset()
        {
            if (_ctrl)
            {
                _ctrl->DecRefWk();
                _ctrl = nullptr;
            }
        }

        safe_obj_weak(const safe_obj_weak& obj)
        {
            if (!obj.expired())
            {
                this->_ctrl = (ControlBlockType*)obj._getControlBlock();
                if (this->_ctrl)
                {
                    this->_ctrl->IncRefWk();
                }
            }
        }

        bool expired() const
        {
            return (this->_ctrl == nullptr || this->_ctrl->get() == nullptr);
        }

        template <class U, class UDealloc,
            typename = std::enable_if_t<
            std::is_convertible_v<U*, T*> && std::is_same_v<LockType::AsStruct, s_safe_obj>
        > >
        safe_obj_weak(const safe_obj<U, UDealloc>& obj)
        {
            this->_ctrl = (ControlBlockType*)obj._getControlBlock();
            if (this->_ctrl)
            {
                this->_ctrl->IncRefWk();
            }
        }

        template <class U, class UDealloc,
            typename = std::enable_if_t<
            std::is_convertible_v<U*, T*> && std::is_same_v<LockType::AsStruct, s_safe_obj_locked>
        > >
        safe_obj_weak(const safe_obj_owned<U, UDealloc>& obj)
        {
            this->_ctrl = (ControlBlockType*)obj._getControlBlock();
            if (this->_ctrl)
            {
                this->_ctrl->IncRefWk();
            }
        }

        // copy assignment
        safe_obj_weak& operator=(const safe_obj_weak& obj)
        {
            reset();

            if (!obj.expired())
            {
                this->_ctrl = obj._ctrl;
                if (this->_ctrl)
                {
                    this->_ctrl->IncRefWk();
                }
            }

            return *this;
        }

        template <class U,
            typename = std::enable_if_t< std::is_convertible_v<U*, T*> >
        >
        safe_obj_weak& operator=(const safe_obj_weak<U, LockType>& obj)
        {
            reset();

            this->_ctrl = (ControlBlockType*)obj._getControlBlock();
            if (this->_ctrl)
            {
                this->_ctrl->IncRefWk();
            }

            return *this;
        }

        ////////////////
        // does not check lock
        operator bool() const
        {
            return (_ctrl != NULL);
        }

        // 
        LockType lock()
        {
            if (_ctrl && _ctrl->get())
            {
                return LockType(
                    (typename LockType::ControlBlockType*)_ctrl);
            }
            return LockType();
        }
    };


}