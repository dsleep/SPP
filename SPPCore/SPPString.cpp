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

	
}