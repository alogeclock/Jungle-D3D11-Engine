#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <queue>
#include <stack>

#include "Engine\Source\Runtime\Core\Public\Math\Vector.h"
#include "Engine\Source\Runtime\Core\Public\Math\Vector4.h"

using FString = std::string;
using uint8 = std::uint8_t;
using int8 = std::int8_t;
using uint16 = std::uint16_t;
using int16 = std::int16_t;
using uint32 = std::uint32_t;
using int32 = std::int32_t;
using uint64 = std::uint64_t;
using int64 = std::int64_t;

template<typename T, typename Alloc = std::allocator<T>>
using TArray = std::vector<T, Alloc>;

struct FVertex 
{
	FVector<float> Position;
	FVector4<float> Color;
};