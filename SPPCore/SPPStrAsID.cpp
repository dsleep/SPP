// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPStrASID.h"
#include "SPPString.h"
#include <unordered_set>
#include <string_view>

namespace SPP
{
	namespace internal
	{
		class GlobalStrToID
		{
		private:
			std::unique_ptr<char[]> ANSI_STR_TABLE;
			const size_t MaxLimit = 10 * 1024 * 1024;
			const size_t MaxIndividualString = 128;
			size_t currentAnsiMemIdx = 1;
			std::unordered_set<std::string_view> ansi_table;

			size_t AddToSet(const std::string_view& InStr)
			{				
				SE_ASSERT(InStr.size() > 0);
				SE_ASSERT((InStr.size() + currentAnsiMemIdx + 1) < MaxLimit);

				auto orgIdx = currentAnsiMemIdx;
				memcpy(ANSI_STR_TABLE.get() + currentAnsiMemIdx, InStr.data(), InStr.size());
				currentAnsiMemIdx += InStr.size();
				ANSI_STR_TABLE[currentAnsiMemIdx] = 0;
				currentAnsiMemIdx++;

				ansi_table.insert(std::string_view(ANSI_STR_TABLE.get() + orgIdx, ANSI_STR_TABLE.get() + currentAnsiMemIdx - 1));
				return orgIdx;
			}

		public:
			GlobalStrToID()
			{
				// 10MB
				ANSI_STR_TABLE.reset( new char[MaxLimit] );
			}

			size_t FindOrAddToSet(const std::string_view &InStr)
			{
				auto mapFind = ansi_table.find(InStr);

				if (mapFind != ansi_table.end())
				{
					return (size_t)(mapFind->data() - ANSI_STR_TABLE.get());
				}

				return AddToSet(InStr);
			}

			std::string_view ToString(size_t InID)
			{
				SE_ASSERT(InID > 0 && InID < currentAnsiMemIdx);

				auto iterIdx = ANSI_STR_TABLE.get() + InID;
				while (*iterIdx) { iterIdx++; }

				return { ANSI_STR_TABLE.get() + InID, iterIdx };
			}

			static GlobalStrToID &Get()
			{
				static GlobalStrToID sO;
				return sO;
			}

		};
	}

	template<typename T>
	bool T_is_number(const T& s)
	{
		typename T::const_iterator it = s.begin();
		while (it != s.end() && std::isdigit(*it)) ++it;
		return !s.empty() && it == s.end();
	}

	template<typename T>
	bool is_allowed_StrAsID(const T& s)
	{
		typename T::const_iterator it = s.begin();
		while (it != s.end() && (std::isdigit(*it) || std::isalpha(*it) || *it == '_' || *it == '.')) ++it;
		return !s.empty() && it == s.end();
	}

	StrAsID::StrAsID(const char* InValue)
	{
		SE_ASSERT(is_allowed_StrAsID(std::string_view(InValue)));

		auto stringValue = std::string(InValue);
		std::inlineToLower(stringValue);

		std::size_t found = stringValue.find_last_of("_");
		if (found != std::string::npos &&
			found <= (stringValue.length() - 2))
		{
			auto numberValue = std::string_view(stringValue.begin() + found + 1, stringValue.end());
			if (T_is_number<std::string_view>(numberValue))
			{
				_counter = std::atoi(numberValue.data());
				_id = (uint32_t)internal::GlobalStrToID::Get().FindOrAddToSet(
					std::string_view(stringValue.begin(), stringValue.begin() + found));
			}
			else
			{
				_id = (uint32_t)internal::GlobalStrToID::Get().FindOrAddToSet(stringValue);
			}
		}
		else
		{
			_id = (uint32_t)internal::GlobalStrToID::Get().FindOrAddToSet(stringValue);
		}		
	}

	StrAsID::StrAsID(const StrAsID& InValue)
	{
		*this = InValue;
	}

	StrAsID& StrAsID::operator=(const StrAsID& InValue)
	{
		_id = InValue._id;
		_counter = InValue._counter;
		return *this;
	}

	std::string StrAsID::ToString() const
	{
		if (_id == 0)
		{
			return "NONE";
		}
		else
		{
			if (_counter >= 0)
			{
				return std::string_format("%s_%d", internal::GlobalStrToID::Get().ToString(_id).data(), _counter);
			}
			else
			{
				return std::string(internal::GlobalStrToID::Get().ToString(_id).data());
			}
		}
	}

}