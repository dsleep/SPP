// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <string>
#include <map>
#include <memory>

namespace SPP
{
	template<class MT>
	typename MT::value_type::second_type* MapFindOrNull(const MT& InMap, const typename MT::value_type::first_type& InKey)
	{
		auto findIter = InMap.find(InKey);
		if (findIter != InMap.end())
		{
			return (MT::value_type::second_type*)&findIter->second;
		}
		return nullptr;
	}

	template<class MT>
	typename MT::value_type::second_type MapFindOrDefault(const MT& InMap, const typename MT::value_type::first_type& InKey)
	{
		auto findIter = InMap.find(InKey);
		if (findIter != InMap.end())
		{
			return (MT::value_type::second_type)findIter->second;
		}
		return MT::value_type::second_type();
	}

	template <typename K, typename V>
	V& MapFindOrAdd(std::map <K, V>& m, const K& key)
	{
		auto it = m.find(key);
		if (it == m.end())
		{
			auto [iter, inserted] = m.insert({ key, V() });
			return iter->second;
		}
		else
		{
			return it->second;
		}
	}

	inline static bool StartsWith(const std::string& s, const std::string& part)
	{
		const size_t partsize = part.size();
		const size_t ssize = s.size();
		if (partsize > ssize) return false;
		for (size_t i = 0; i < partsize; i++) {
			if (s[i] != part[i]) return false;
		}
		return true;
	}
	inline static bool EndsWith(const std::string& s, const std::string& part)
	{
		const size_t partsize = part.size();
		const size_t ssize = s.size();
		if (partsize > ssize) return false;
		for (size_t i = 0; i < partsize; i++) {
			if (s[ssize - partsize + i] != part[i]) return false;
		}
		return true;
	}

	template<typename T>
	void EraseWithBackSwap(uint32_t Idx, std::vector<T> &InVec)
	{
		SE_ASSERT(Idx < InVec.size());

		if (InVec.size() > 1)
		{
			std::iter_swap(InVec.begin() + Idx, InVec.end() - 1);
			InVec.pop_back();
		}
		else
		{
			InVec.clear();
		}
	}

	class MultipleInheritableEnableSharedFromThis : public std::enable_shared_from_this<MultipleInheritableEnableSharedFromThis>
	{
	public:
		virtual ~MultipleInheritableEnableSharedFromThis()
		{}
	};

	template <class T>
	class inheritable_enable_shared_from_this : virtual public MultipleInheritableEnableSharedFromThis
	{
	public:
		std::shared_ptr<T> shared_from_this() 
		{
			return std::dynamic_pointer_cast<T>(MultipleInheritableEnableSharedFromThis::shared_from_this());
		}
		template <class Down>
		std::shared_ptr<Down> downcasted_shared_from_this() 
		{
			return std::dynamic_pointer_cast<Down>(MultipleInheritableEnableSharedFromThis::shared_from_this());
		}
	};

	template <typename T>
	struct IteratorWithNumeric
	{
		T& list;
		typedef decltype(list.begin()) I;

		struct InnerIterator
		{
			size_t curIdx = 0;
			I i;
			InnerIterator(I i) : i(i) {}
			std::tuple<I, size_t> operator * ()
			{
				return std::tuple< I, size_t>{i, curIdx};
			}
			I operator ++ ()
			{
				curIdx++;
				return ++i;
			}
			bool operator != (const InnerIterator& o) { return i != o.i; }
		};

		IteratorWithNumeric(T& list) : list(list) {}
		InnerIterator begin() { return InnerIterator(list.begin()); }
		InnerIterator end() { return InnerIterator(list.end()); }
	};
}