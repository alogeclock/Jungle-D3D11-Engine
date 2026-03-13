#pragma once

#include "PlatformCrt.h"

struct FGenericPlatformMath
{
	static float Sin(float Value) { return sinf(Value); }
	static float Cos(float Value) { return cosf(Value); }
	static float Tan(float Value) { return tanf(Value); }
	static float Sqrt(float Value) { return sqrtf(Value); }
	static float InvSqrt(float F) { return 1.0f / sqrtf(F); }
};