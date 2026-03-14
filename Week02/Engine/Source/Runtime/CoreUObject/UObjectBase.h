#pragma once

#include "NameTypes.h"
#include "ObjectMacros.h"

class UClass;
class UPackage;

class UObjectBase
{
public:

	uint32 GetUniqueID() const
	{
		return (uint32)InternalIndex;
	}

private:

	int32							InternalIndex;
};