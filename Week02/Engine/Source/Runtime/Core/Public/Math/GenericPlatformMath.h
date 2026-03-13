#pragma once

#include "PlatformCrt.h"

struct FGenericPlatformMath
{
	static float Tan(float Value) { return tanf(Value); }
	static float Sqrt(float Value) { return sqrtf(Value); }
};