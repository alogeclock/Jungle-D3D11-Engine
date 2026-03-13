#pragma once

#include "../Math/PlatformMath.h"

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