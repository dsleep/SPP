// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPString.h"
#include <locale>
#include <codecvt>
#include <ostream>
#include <optional>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <random>

namespace std
{
	void inlineToLower(std::string& InData)
	{
		std::transform(InData.begin(), InData.end(), InData.begin(), [](unsigned char c) { return std::tolower(c); });
	}
	void inlineToUpper(std::string& InData)
	{
		std::transform(InData.begin(), InData.end(), InData.begin(), [](unsigned char c) { return std::toupper(c); });
	}

	std::string str_to_upper(std::string& InData)
	{
		std::string oString = InData;
		inlineToUpper(oString);
		return oString;
	}

	unsigned int random_char()
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 255);
		return dis(gen);
	}

	std::string generate_hex(const uint8_t len)
	{
		std::stringstream ss;
		for (auto i = 0; i < len; i++)
		{
			const auto rc = random_char();
			std::stringstream hexstream;
			hexstream << std::hex << rc;
			auto hex = hexstream.str();
			ss << (hex.length() < 2 ? '0' + hex : hex);
		}
		return ss.str();
	}

    // convert UTF-8 string to wstring
    std::wstring utf8_to_wstring(const std::string& str) 
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
        return myconv.from_bytes(str);
    }

    // convert wstring to UTF-8 string
    std::string wstring_to_utf8(const std::wstring& str) 
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
        return myconv.to_bytes(str);
    }

	std::vector<std::string> str_split(const std::string& s, char delim)
	{
		std::vector<std::string> result;
		std::stringstream ss(s);
		std::string item;

		while (std::getline(ss, item, delim)) {
			result.push_back(item);
		}

		return result;
	}

	bool str_equals(const std::string& a, const std::string& b, bool bIgnoreCase)
	{
		return std::equal(a.begin(), a.end(),
			b.begin(), b.end(),
			[bIgnoreCase](char a, char b) {
				return bIgnoreCase ? std::tolower(a) == std::tolower(b) : a == b;
			});
	}

	bool is_number(const std::string& s)
	{
		std::string::const_iterator it = s.begin();
		while (it != s.end() && std::isdigit(*it)) ++it;
		return !s.empty() && it == s.end();
	}

	bool is_alphanumeric(const std::string& s)
	{
		std::string::const_iterator it = s.begin();
		while (it != s.end() && (std::isdigit(*it) || std::isalpha(*it))) ++it;
		return !s.empty() && it == s.end();
	}

	//////////////

	const std::string WHITESPACE = " \n\r\t\f\v";

	std::string ltrim(const std::string& s)
	{
		size_t start = s.find_first_not_of(WHITESPACE);
		return (start == std::string::npos) ? "" : s.substr(start);
	}

	std::string rtrim(const std::string& s)
	{
		size_t end = s.find_last_not_of(WHITESPACE);
		return (end == std::string::npos) ? "" : s.substr(0, end + 1);
	}

	std::string trim(const std::string& s) {
		return rtrim(ltrim(s));
	}

	std::map<std::string, std::string> BuildCCMap(int argc, char* argv[])
	{
		std::map<std::string, std::string> oMap;

		for (int ArgIter = 0; ArgIter < argc; ArgIter++)
		{
			std::string curArg = argv[ArgIter];

			auto trimmed = trim(curArg);

			if (trimmed.length() > 1 && trimmed[0] == '-')
			{
				auto curEquals = trimmed.find_first_of('=');

				if (curEquals == std::string::npos)
				{
					auto curKey = trimmed.substr(1);
					oMap[curKey] = "";
				}
				else if (curEquals != std::string::npos &&
					curEquals > 1)
				{
					auto curKey = trimmed.substr(1, curEquals - 1);
					auto curValue = trimmed.substr(curEquals + 1);
					oMap[curKey] = curValue;
				}
			}
		}

		return oMap;
	}
}