// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <stdexcept>

namespace std
{
	SPP_CORE_API void inlineToLower(std::string& InData);
	SPP_CORE_API void inlineToUpper(std::string& InData);

	SPP_CORE_API std::string str_to_upper(const std::string& InData);

	SPP_CORE_API unsigned int random_char();

	SPP_CORE_API std::string generate_hex(const uint8_t len);

    // convert UTF-8 string to wstring
    SPP_CORE_API std::wstring utf8_to_wstring(const std::string& str);
    // convert wstring to UTF-8 string
    SPP_CORE_API std::string wstring_to_utf8(const std::wstring& str);

	SPP_CORE_API std::vector<std::string> str_split(const std::string& s, char delim);

    SPP_CORE_API bool str_equals(const std::string& a, const std::string& b, bool bIgnoreCase = true);

	SPP_CORE_API bool is_alphanumeric(const std::string& s);

	SPP_CORE_API bool is_number(const std::string& s);

	template<typename ... Args>
	inline std::string string_format(const std::string& format, Args ... args)
	{
		size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
		if (size <= 0) { throw std::runtime_error("Error during formatting."); }
		std::unique_ptr<char[]> buf(new char[size]);
		snprintf(buf.get(), size, format.c_str(), args ...);
		return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
	}

	SPP_CORE_API std::string ReplaceAll(std::string str, const std::string& from, const std::string& to);
	SPP_CORE_API void ReplaceInline(std::string& str, const std::string& from, const std::string& to);

	SPP_CORE_API std::string ltrim(const std::string& s);
	SPP_CORE_API std::string rtrim(const std::string& s);
	SPP_CORE_API std::string trim(const std::string& s);

	SPP_CORE_API std::map<std::string, std::string> BuildCCMap(int argc, char* argv[]);
}