#pragma once

#include "UObjectBase.h"

class FUObjectArray
{
	friend class UObject;
	friend UObject* StaticAllocateObject(const UClass*, UObject*, FName, EObjectFlags, EInternalObjectFlags, bool, bool*, UPackage*, int32, FRemoteObjectId, class FGCReconstructionGuard*);

};

extern FUObjectArray GUObjectArray;