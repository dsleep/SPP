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
	class SPP_CORE_API NumberedString
	{
	private:
		uint32_t _id = 0;
		uint32_t _counter = ~((uint32_t)0);
		const char* _name = nullptr;

	public:
		NumberedString(const char* InValue);

		uint32_t GetID() const
		{
			return _id;
		}

		const char* GetValue() const
		{
			return _name;
		}	

		bool operator==(const NumberedString& cmpTo) const
		{
			return GetID() == cmpTo.GetID();
		}

		bool operator!=(const NumberedString& cmpTo) const
		{
			return GetID() != cmpTo.GetID();
		}

		bool operator<(const NumberedString& cmpTo) const
		{
			return GetID() < cmpTo.GetID();
		}

		struct HASH
		{
			size_t operator()(const NumberedString& InValue) const
			{
				return InValue.GetID();
			}
		};
	};		
}