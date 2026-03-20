#include "Guid.h"

#include "PlatformMisc.h"

FGuid FGuid::NewGuid()
{
	FGuid Result(0, 0, 0, 0);
	FPlatformMisc::CreateGuid(Result);

	return Result;
}