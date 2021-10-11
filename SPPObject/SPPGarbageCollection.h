// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPObject.h"

namespace SPP
{
	SPP_OBJECT_API void AddToRoot(SPPObject *InObject);
	SPP_OBJECT_API void RemoveFromRoot(SPPObject* InObject);

	SPP_OBJECT_API void GC_MarkAndSweep();
}