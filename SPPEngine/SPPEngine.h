// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"
#include <string>

#if _WIN32 && !defined(SPP_ENGINE_STATIC)

	#ifdef SPP_ENGINE_EXPORT
		#define SPP_ENGINE_API __declspec(dllexport)
	#else
		#define SPP_ENGINE_API __declspec(dllimport)
	#endif

	#else

		#define SPP_ENGINE_API 

#endif



namespace SPP
{
	SPP_ENGINE_API extern std::string GAssetPath;

	class SPP_ENGINE_API AssetPath
	{
	private:		
		std::string _relPath;
		std::string _finalPath;

	public:
		AssetPath() = default;
		AssetPath(const char* InRelPath);
		const char* operator *() const;
		std::string GetExtension() const;
		std::string GetName() const;
		std::string GetRelativePath() const;
	};
}

