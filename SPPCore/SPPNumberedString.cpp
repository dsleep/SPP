// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPNumberedString.h"
#include "SPPString.h"
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <memory>
#include <functional>
#include <mutex>

namespace SPP
{
	namespace internal
	{
		static std::mutex MapLock;
		static uint32_t nameIdx = 1;

		static std::map<std::string, uint32_t>& GetNameToID()
		{
			static std::map<std::string, uint32_t> sO;
			return sO;
		}

		static std::map<uint32_t, const char*>& GetIDToName()
		{
			static std::map<uint32_t, const char*> sO;
			return sO;
		}
	}	

	NumberedString::NumberedString(const char* InValue)
	{
		std::unique_lock<std::mutex> lk(internal::MapLock);

		auto &NameToID = internal::GetNameToID();
		auto &IDToName = internal::GetIDToName();

		auto stringValue = std::string(InValue);
		std::inlineToLower(stringValue);

		std::size_t found = stringValue.find_last_of("_");
		if (found != std::string::npos && 
			found <= (stringValue.length() - 2))
		{
			auto numberValue = stringValue.substr(found + 1);
			if (std::is_number(numberValue))
			{
				stringValue = stringValue.substr(0, found);
				_counter = std::atoi(numberValue.c_str());
			}
		}

		const auto [it, success] = NameToID.insert({ stringValue, internal::nameIdx });
		if (success)
		{
			//was inserted
			IDToName[internal::nameIdx] = it->first.c_str();
			internal::nameIdx++;
		}
				
		_name = it->first.c_str();
		_id = it->second;
	}

}