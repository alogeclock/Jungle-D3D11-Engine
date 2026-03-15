#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <list>
#include <queue>
#include <stack>

#include "Memory/Memory.h"
#include "Engine\Source\Runtime\Core\Public\Math\Vector.h"
#include "Engine\Source\Runtime\Core\Public\Math\Vector4.h"

using int32 = std::int32_t;
using uint32 = std::uint32_t;

using uint8 = std::uint8_t;
using int8 = std::int8_t;
using uint16 = std::uint16_t;
using int16 = std::int16_t;
using uint64 = std::uint64_t;
using int64 = std::int64_t;

template<typename T, typename Alloc = std::allocator<T>>
using TArray = std::vector<T, Alloc>;
using FString = std::string;
template<typename K, typename V> using Tmap = std::unordered_map<K, V>;
template<typename T> using TSet = std::unordered_set<T>;
template<typename T> using TQueue = std::queue<T>;

struct FVertex
{
	FVector<float> Position;
	FVector4<float> Color;
};