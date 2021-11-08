// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <functional>
#include <map>

#if _WIN32 && !defined(SPP_REFLECTION_STATIC)

	#ifdef SPP_WINRTBTW_EXPORT
		#define SPP_WINRTBTE_API __declspec(dllexport)
	#else
		#define SPP_WINRTBTE_API __declspec(dllimport)
	#endif

#else

	#define SPP_WINRTBTE_API 

#endif


namespace SPP
{	
	SPP_WINRTBTE_API uint32_t GetWinRTBTWVersion();

	class SPP_WINRTBTE_API BTEWatcher
	{
	private:
		struct PlatImpl;
		std::unique_ptr<PlatImpl> _impl;

	public:
		BTEWatcher();
		~BTEWatcher();
		void WatchForData(const std::string& DeviceData, const std::map< std::string, std::function<void(uint8_t*, size_t)> >& CharacterFunMap);
	};
}