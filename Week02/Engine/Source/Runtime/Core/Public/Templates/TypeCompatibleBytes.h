#pragma once

#include <string.h>

template <typename Totype, typename Fromtype>
inline Totype BitCast(const Fromtype& From)
{
	Totype Result;
	memcpy(&Result, &From, sizeof(Totype));
	return Result;
}
