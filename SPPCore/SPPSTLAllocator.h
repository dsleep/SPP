#pragma once


//template <class T>
//class MSpace_Allocator
//{
//public:
//
//	// type definitions
//	typedef T        value_type;
//	typedef T* pointer;
//	typedef const T* const_pointer;
//	typedef T& reference;
//	typedef const T& const_reference;
//	typedef std::size_t    size_type;
//	typedef std::ptrdiff_t difference_type;
//
//	// rebind allocator to type U
//	template <class U>
//	struct rebind {
//		typedef MSpace_Allocator<U> other;
//	};
//
//	// return address of values
//	pointer address(reference value) const {
//		return &value;
//	}
//	const_pointer address(const_reference value) const {
//		return &value;
//	}
//
//	/* constructors and destructor
//	  * - nothing to do because the allocator has no state
//	  */
//	MSpace_Allocator() throw() 
//	{
//	}
//	MSpace_Allocator(const MSpace_Allocator&) throw() 
//	{
//	}
//	template <class U>
//	MSpace_Allocator(const MSpace_Allocator<U>&) throw()
//	{
//	}
//	~MSpace_Allocator() throw()
//	{
//	}
//
//	// return maximum number of elements that can be allocated
//	size_type max_size() const throw() 
//	{
//		return std::numeric_limits<std::size_t>::max() / sizeof(T);
//	}
//
//	// allocate but don't initialize num elements of type T
//	pointer allocate(size_type num, const void* = 0)
//	{
//		// print message and allocate memory with global new
//		std::cerr << "allocate " << num << " element(s)"
//			<< " of size " << sizeof(T) << std::endl;
//		pointer ret = (pointer)(::operator new(num * sizeof(T)));
//		std::cerr << " allocated at: " << (void*)ret << std::endl;
//		return ret;
//	}
//
//	// initialize elements of allocated storage p with value value
//	void construct(pointer p, const T& value)
//	{
//		// initialize memory with placement new
//		new((void*)p)T(value);
//	}
//
//	// destroy elements of initialized storage p
//	void destroy(pointer p) 
//	{
//		// destroy objects by calling their destructor
//		p->~T();
//	}
//
//	// deallocate storage p of deleted elements
//	void deallocate(pointer p, size_type num) 
//	{
//		// print message and deallocate memory with global delete
//		std::cerr << "deallocate " << num << " element(s)"
//			<< " of size " << sizeof(T)
//			<< " at: " << (void*)p << std::endl;
//		::operator delete((void*)p);
//	}
//};

#pragma once

#include <functional>

template <typename T>
class stack_allocator 
{
    template<typename> friend class stack_allocator;

public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;

    template<typename T2>
    struct rebind {
        typedef stack_allocator<T2> other;
    };

private:
    T* ptr;
    size_t currentSize, maxSize;

public:
    stack_allocator() noexcept :
        ptr(nullptr),
        currentSize(0),
        maxSize(0) {
    }

    stack_allocator(T* buffer, size_t size) noexcept :
        ptr(buffer),
        currentSize(0),
        maxSize(size) {
    }

    template <typename T2>
    explicit stack_allocator(const stack_allocator<T2>& other) noexcept :
        ptr(reinterpret_cast<T*>(other.ptr)),
        currentSize(other.currentSize),
        maxSize(other.maxSize) {
    }

    T* allocate(size_t n, const void* hint = nullptr) {
        T* pointer = ptr + currentSize;
        currentSize += n;
        return pointer;
    }

    void deallocate(T* p, size_t n) {
        currentSize -= n;
    }

    size_t capacity() const noexcept {
        return maxSize;
    }

    size_t max_size() const noexcept {
        return maxSize;
    }

    T* address(T& x) const noexcept {
        return &x;
    }

    const T* address(const T& x) const noexcept {
        return &x;
    }

    T* buffer() const noexcept {
        return ptr;
    }

    template <typename T2>
    stack_allocator& operator=(const stack_allocator<T2>& alloc) {
        return *this;
    }

    template <typename... Args>
    void construct(T* p, Args&&... args) {
        new (p) T(forward<Args>(args)...);
    }

    void destroy(T* p) {
        p->~T();
    }

    template <typename T2>
    bool operator==(const stack_allocator<T2>& other) const noexcept {
        return ptr == other.ptr;
    }

    template <typename T2>
    bool operator!=(const stack_allocator<T2>& other) const noexcept {
        return ptr != other.ptr;
    }
};

//#define init_stack_vector(Type, Name, Size) std::vector<Type, std::stack_allocator<Type>> Name((std::stack_allocator<Type>(reinterpret_cast<Type*>(alloca(Size * sizeof(Type))), Size))); Name.reserve(Size)