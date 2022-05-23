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

	AllocateObject<OScene>("Scene0", nullptr);
	AllocateObject<OSDFBox>("dummyBox", nullptr);
	AllocateObject<OScene>("Scene5", nullptr);

	auto EntityScene = AllocateObject<OScene>("Scene", nullptr);


	auto CurrentEntity = AllocateObject<OShapeGroup>("ShapeGroup", EntityScene);
	auto CurrentObject = AllocateObject<OSDFBox>("Box", CurrentEntity);
	
	CurrentEntity->AddChild(CurrentObject);
	EntityScene->AddChild(CurrentEntity);

	AddToRoot(EntityScene);

	//RESET 
	IterateObjects([](SPPObject *InObj) -> bool
		{
			SPP_LOG(LOG_APP, LOG_INFO, "object %s", InObj->GetName());
			return true;
		});
	
	GC_MarkAndSweep();

	IterateObjects([](SPPObject* InObj) -> bool
		{
			SPP_LOG(LOG_APP, LOG_INFO, "still remains object %s", InObj->GetName());
			return true;
		});
	EntityScene->RemoveChild(CurrentEntity);

	GC_MarkAndSweep();

	IterateObjects([](SPPObject* InObj) -> bool
		{
			SPP_LOG(LOG_APP, LOG_INFO, "still remains!!! object %s", InObj->GetName());
			return true;
		});

	RemoveFromRoot(EntityScene);
	GC_MarkAndSweep();

	IterateObjects([](SPPObject* InObj) -> bool
		{
			SPP_LOG(LOG_APP, LOG_INFO, "SHOULD NOT REMAIN object % s", InObj->GetName());
			return true;
		});

	return 0;
}
