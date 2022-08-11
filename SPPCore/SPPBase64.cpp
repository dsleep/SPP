// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

// originally written by René Nyffenegger https://renenyffenegger.ch/notes/development/Base64/Encoding-and-decoding-base-64-with-cpp/
// taken modifications from Michal Lihocký https://stackoverflow.com/questions/180947/base64-decode-snippet-in-c
// and other mods from polfosol

#pragma once

#include "SPPBase64.h"
#include <iostream>

inline
uint8_t const*
get_base64_chars()
{
    static uint8_t constexpr tab[] = {
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    };
    return &tab[0];
}

inline
uint8_t const*
get_base64_chars_inverse()
{
    static uint8_t constexpr tab[] = {
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, //   0-15
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, //  16-31
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 255, 255, 63, //  32-47
         52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 255, 255, 255, 255, //  48-63
         255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, //  64-79
         15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 255, //  80-95
         255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, //  96-111
         41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255, // 112-127
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 128-143
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 144-159
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 160-175
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 176-191
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 192-207
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 208-223
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 224-239
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255  // 240-255
    };
    return &tab[0];
}

static inline bool is_base64(uint8_t c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}

namespace SPP
{
	std::string base64_encode(uint8_t const* buf, size_t bufLen)
	{
        auto to_base64 = get_base64_chars();
        // Calculate how many bytes that needs to be added to get a multiple of 3
        size_t missing = 0;
        size_t ret_size = bufLen;
        while ((ret_size % 3) != 0)
        {
            ++ret_size;
            ++missing;
        }

        // Expand the return string size to a multiple of 4
        ret_size = 4 * ret_size / 3;

        std::string ret;
        ret.reserve(ret_size);

        for (unsigned int i = 0; i < ret_size / 4; ++i)
        {
            // Read a group of three bytes (avoid buffer overrun by replacing with 0)
            size_t index = i * 3;
            uint8_t b3[3];
            b3[0] = (index + 0 < bufLen) ? buf[index + 0] : 0;
            b3[1] = (index + 1 < bufLen) ? buf[index + 1] : 0;
            b3[2] = (index + 2 < bufLen) ? buf[index + 2] : 0;

            // Transform into four base 64 characters
            uint8_t b4[4];
            b4[0] = ((b3[0] & 0xfc) >> 2);
            b4[1] = ((b3[0] & 0x03) << 4) + ((b3[1] & 0xf0) >> 4);
            b4[2] = ((b3[1] & 0x0f) << 2) + ((b3[2] & 0xc0) >> 6);
            b4[3] = ((b3[2] & 0x3f) << 0);

            // Add the base 64 characters to the return value
            ret.push_back(to_base64[b4[0]]);
            ret.push_back(to_base64[b4[1]]);
            ret.push_back(to_base64[b4[2]]);
            ret.push_back(to_base64[b4[3]]);
        }

        // Replace data that is invalid (always as many as there are missing bytes)
        for (size_t i = 0; i < missing; ++i)
            ret[ret_size - i - 1] = '=';

        SE_ASSERT((ret.size() % 4) == 0);

        return ret;
    }

    std::vector<uint8_t> base64_decode(std::string const& encoded_string)
    {
        SE_ASSERT((encoded_string.size() % 4) == 0);

        auto from_base64 = get_base64_chars_inverse();

        // Make sure the *intended* string length is a multiple of 4
        size_t encoded_size = encoded_string.size();

        while ((encoded_size % 4) != 0)
            ++encoded_size;

        std::vector<uint8_t> ret;
        ret.reserve(3 * encoded_size / 4);

        for (size_t i = 0; i < encoded_size; i += 4)
        {
            // Get values for each group of four base 64 characters
            uint8_t b4[4];
            b4[0] = (encoded_string[i + 0] <= 'z') ? from_base64[encoded_string[i + 0]] : 0xff;
            b4[1] = (encoded_string[i + 1] <= 'z') ? from_base64[encoded_string[i + 1]] : 0xff;
            b4[2] = (encoded_string[i + 2] <= 'z') ? from_base64[encoded_string[i + 2]] : 0xff;
            b4[3] = (encoded_string[i + 3] <= 'z') ? from_base64[encoded_string[i + 3]] : 0xff;

            // Transform into a group of three bytes
            uint8_t b3[3];
            b3[0] = ((b4[0] & 0x3f) << 2) + ((b4[1] & 0x30) >> 4);
            b3[1] = ((b4[1] & 0x0f) << 4) + ((b4[2] & 0x3c) >> 2);
            b3[2] = ((b4[2] & 0x03) << 6) + ((b4[3] & 0x3f) >> 0);

            // Add the byte to the return value if it isn't part of an '=' character (indicated by 0xff)
            if (b4[1] != 0xff) ret.push_back(b3[0]);
            if (b4[2] != 0xff) ret.push_back(b3[1]);
            if (b4[3] != 0xff) ret.push_back(b3[2]);
        }

        return ret;
    }
}