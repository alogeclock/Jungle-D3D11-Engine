#pragma once

#include "PlatformCrt.h"
#include "../Templates/TypeCompatibleBytes.h"

struct FGenericPlatformMath
{
	static float Sin(float Value) { return sinf(Value); }
	static float Cos(float Value) { return cosf(Value); }
	static float Tan(float Value) { return tanf(Value); }
	static float Sqrt(float Value) { return sqrtf(Value); }
	static float InvSqrt(float F) { return 1.0f / sqrtf(F); }

	static bool IsFinite(float A)
	{
		return (BitCast<uint32, float>(A) & 0x7F800000U) != 0x7F800000U;
	}
};