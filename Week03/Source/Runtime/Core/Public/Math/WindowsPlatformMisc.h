#pragma once

#include "GenericPlatformMisc.h"

struct FWindowsPlatformMisc : public FGenericPlatformMisc
{
	static void CreateGuid(struct FGuid& Result);
};

typedef FWindowsPlatformMisc FPlatformMisc;