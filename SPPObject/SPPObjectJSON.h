// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPObject.h"
#include "json/json.h"

namespace SPP
{
	SPP_OBJECT_API void ObjectToJSON(const SPPObject* InObject, Json::Value& CurrentContainer);
}