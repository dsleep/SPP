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
	template<typename K, typename V>
	V* MapFindOrNull(const std::map<K, V>& InMap, const K& InKey)
	{
		auto findIter = InMap.find(InKey);
		if (findIter != InMap.end())
		{
			return (V*)&findIter->second;
		}
		return nullptr;
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
	void EraseUnordered(uint32_t Index, std::vector<T> &InVec)
	{
		if (InVec.size() > 1)
		{
			std::iter_swap(InVec.begin() + index, InVec.end() - 1);
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
}