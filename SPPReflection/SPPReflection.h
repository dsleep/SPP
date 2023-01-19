// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPCore.h"

#include "json/json.h"

#include <rttr/registration>
#include <rttr/registration_friend>

#if _WIN32 && !defined(SPP_REFLECTION_STATIC)

	#ifdef SPP_REFLECTION_EXPORT
		#define SPP_REFLECTION_API __declspec(dllexport)
	#else
		#define SPP_REFLECTION_API __declspec(dllimport)
	#endif

#else

	#define SPP_REFLECTION_API 

#endif


namespace SPP
{	
	SPP_REFLECTION_API uint32_t GetReflectionVersion();

	SPP_REFLECTION_API bool SetPropertyValue(const rttr::instance& obj, rttr::property& curPoperty, const std::string& InValue);
	SPP_REFLECTION_API void PODToJSON(const rttr::variant& inValue, Json::Value& JsonRoot);
	SPP_REFLECTION_API void JSONToPOD(const rttr::variant& inValue, const Json::Value& JsonRoot);
}