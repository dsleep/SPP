// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <vector>
#include <list>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>

namespace SPP
{
	class SPP_CORE_API StrAsID
	{
	private:
		uint32_t _id = 0;
		int32_t _counter = -1;

	public:
		StrAsID(const char* InValue);
		StrAsID(const StrAsID& InValue);
		StrAsID &operator=(const StrAsID &InValue);

		uint32_t GetID() const
		{
			return _id;
		}

		uint32_t GetNumber() const
		{
			return _id;
		}

		std::string ToString() const;

		bool operator==(const StrAsID& cmpTo) const
		{
			return GetID() == cmpTo.GetID() && GetNumber() == cmpTo.GetNumber();
		}

		struct HASH
		{
			size_t operator()(const StrAsID& InValue) const
			{
				std::size_t h = 0;
				std::hash_combine(h, InValue.GetID(), InValue.GetNumber());
				return h;
			}
		};
	};		
}