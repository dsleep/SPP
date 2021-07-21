// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <type_traits>
	
namespace SPP
{
 	template<typename T>
	class Array
	{		
		//using IsTrivialConstructible = std::is_trivially_constructible<T>;
		//using IsTrivialDestructible = std::is_trivially_destructible<T>;

	protected:
		T* _data = nullptr;
		uint64_t _elementCount = 0;
		uint64_t _reservedCount = 0;

	public:
		T& PushBack(const T &InValue)
		{
			EmplaceBack(InValue);
		}
		T& PushBack(T&& InValue)
		{
			EmplaceBack(std::forward(InValue));
		}

		template <class... Args>
		void EmplaceBack(Args&&... args)
		{
			auto currentIdx = _elementCount;
			Expand(1);
			new(GetData() + currentIdx) T(std::forward<Args>(args)...);
		}

		void Expand(uint64_t InSize)
		{
			auto NewCount = _elementCount + InSize;
			if (NewCount > _reservedCount)
			{
				ResizeReserve(_reservedCount * 2);
			}
			_elementCount += NewCount;
		}

		T* GetData() const
		{
			return _data;
		}

		void ResizeReserve(uint64_t InSize)
		{			
			if (InSize != _reservedCount)
			{
				T* newData = malloc(InSize);

				std::copy(GetData(), GetData() + _elementCount, newData);
				//new(newData + currentIdx) T(GetData() + currentIdx);
			}
		}

		~Array()
		{
			Clear();
		}

		void Clear()
		{
			if constexpr (IsTrivialDestructible == false)
			{

			}
		}
	};
}