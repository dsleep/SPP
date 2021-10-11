// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPCore.h"
#include "SPPLogging.h"
#include "SPPObject.h"
#include "SPPSDFO.h"
#include "SPPJsonUtils.h"
#include "SPPGarbageCollection.h"

using namespace SPP;

LogEntry LOG_APP("APP");


int main(int argc, char* argv[])
{
	IntializeCore(nullptr);
	GetSDFVersion();

	NumberedString testString("bigValue_0");

	AllocateObject<OScene>("Scene0");
	AllocateObject<OSDFBox>("dummyBox");
	AllocateObject<OScene>("Scene5");

	auto EntityScene = AllocateObject<OScene>("Scene");

	auto CurrentObject = AllocateObject<OSDFBox>("World.ShapeGroup_0.Box_0");
	auto CurrentEntity = AllocateObject<OShapeGroup>("World.ShapeGroup_0");
	
	CurrentEntity->AddChild(CurrentObject);
	EntityScene->AddChild(CurrentEntity);

	AddToRoot(EntityScene);

	//RESET 
	IterateObjects([](SPPObject *InObj) -> bool
		{
			SPP_LOG(LOG_APP, LOG_INFO, "object %s", InObj->GetPath().ToString().c_str());
			return true;
		});
	
	GC_MarkAndSweep();

	IterateObjects([](SPPObject* InObj) -> bool
		{
			SPP_LOG(LOG_APP, LOG_INFO, "still remains object %s", InObj->GetPath().ToString().c_str());
			return true;
		});
	EntityScene->RemoveChild(CurrentEntity);

	GC_MarkAndSweep();

	IterateObjects([](SPPObject* InObj) -> bool
		{
			SPP_LOG(LOG_APP, LOG_INFO, "still remains!!! object %s", InObj->GetPath().ToString().c_str());
			return true;
		});

	RemoveFromRoot(EntityScene);
	GC_MarkAndSweep();

	IterateObjects([](SPPObject* InObj) -> bool
		{
			SPP_LOG(LOG_APP, LOG_INFO, "SHOULD NOT REMAIN object % s", InObj->GetPath().ToString().c_str());
			return true;
		});

	return 0;
}
