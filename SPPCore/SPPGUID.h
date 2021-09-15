// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include "SPPString.h"

namespace std
{
	template <typename T, typename... Rest>
	void hash_combine(std::size_t& seed, const T& v, const Rest&... rest)
	{
		seed ^= std::hash<T>{}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2);
		(hash_combine(seed, rest), ...);
	}
}

namespace SPP
{
	struct SPP_CORE_API GUID
	{
	private:
		uint32_t A = 0;
		uint32_t B = 0;
		uint32_t C = 0;
		uint32_t D = 0;

	public:
		GUID() = default;

		GUID(const char* InString);

		GUID(uint32_t iA,
		uint32_t iB,
		uint32_t iC,
		uint32_t iD) : A(iA), B(iB), C(iC), D(iD) { }
		
		uint32_t& operator[](size_t Index);

		bool operator==(const GUID& Other) const
		{
			return (A == Other.A &&
				B == Other.B &&
				C == Other.C &&
				D == Other.D);
		}

		std::size_t Hash() const
		{ 
			static_assert(sizeof(std::size_t) == sizeof(uint64_t));
			std::size_t h = 0;
			std::hash_combine(h, A,B,C,D);
			return h;
		};

		std::string ToString() const;

		static GUID Create();
	};
}

namespace std
{
	template<> struct less<SPP::GUID>
	{
		bool operator() (const SPP::GUID& lhs, const SPP::GUID& rhs) const
		{
			return memcmp(&lhs, &rhs, sizeof(SPP::GUID)) < 0;
		}
	};

	template <> struct hash<SPP::GUID>
	{ 
		size_t operator()(const SPP::GUID& v) const
		{ 
			return v.Hash();
		} 
	};
}
