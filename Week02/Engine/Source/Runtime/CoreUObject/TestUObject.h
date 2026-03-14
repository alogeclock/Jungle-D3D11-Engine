#pragma once

#include "../Core/Public/Math/Platform.h"

class UObject
{
public:
	uint32 UUID;
	uint32 InternalIndex; // Index of GUObjectArray
};

//TArray<UObject*> GUObjectArray;