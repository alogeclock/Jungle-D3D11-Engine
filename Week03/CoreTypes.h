#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <queue>
#include <stack>
#include <cmath>
#include <algorithm>

#include "Source/Core/Public/Math/Vector.h"
#include "Source/Core/Public/Math/Vector4.h"

class UWorld;
extern UWorld *GWorld;

using int32 = std::int32_t;
using uint32 = std::uint32_t;

using uint8 = std::uint8_t;
using int8 = std::int8_t;
using uint16 = std::uint16_t;
using int16 = std::int16_t;
using uint64 = std::uint64_t;
using int64 = std::int64_t;

template <typename T, typename Alloc = std::allocator<T>> using TArray = std::vector<T, Alloc>;
using FString = std::string;
template <typename KeyType, typename ValueType, typename Hash = std::hash<KeyType>, typename Eq = std::equal_to<KeyType>,
          typename Alloc = std::allocator<std::pair<const KeyType, ValueType>>>
using TMap = std::unordered_map<KeyType, ValueType, Hash, Eq, Alloc>;
template <typename T> using TSet = std::unordered_set<T>;
template <typename T> using TQueue = std::queue<T>;

struct FVertex
{
    FVector<float>  Position;
    FVector4<float> Color;
};

struct FVertexToTexture
{
    FVector<float> Position;
    float          u, v;
};

enum class EPrimitiveType : uint8
{
    None,
    Sphere,
    Cube,
    Triangle,
    Plane,
    Arrow,
    CubeArrow,
    Ring,
    Axis,
    Grid,
    WireBox,
    Text,
};

enum class ECullMode : uint8
{
    None,
    Front,
    Back
};

enum class EViewModeIndex : uint8
{
    VMI_Lit,
    VMI_Unlit,
    VMI_Wireframe
};

enum class EEngineShowFlags : uint64
{
    None,
    SF_Primitives = 1ULL << 0,    // 0001 (1번째 체크박스)
    SF_BillboardText = 1ULL << 1, // 0010 (2번째 체크박스)
};

inline EEngineShowFlags operator|(EEngineShowFlags Lhs, EEngineShowFlags Rhs)
{
    return static_cast<EEngineShowFlags>(static_cast<uint64>(Lhs) | static_cast<uint64>(Rhs));
}

inline EEngineShowFlags &operator|=(EEngineShowFlags &Lhs, EEngineShowFlags Rhs)
{
    Lhs = Lhs | Rhs;
    return Lhs;
}

inline EEngineShowFlags operator&(EEngineShowFlags Lhs, EEngineShowFlags Rhs)
{
    return static_cast<EEngineShowFlags>(static_cast<uint64>(Lhs) & static_cast<uint64>(Rhs));
}

inline EEngineShowFlags &operator&=(EEngineShowFlags &Lhs, EEngineShowFlags Rhs)
{
    Lhs = Lhs & Rhs;
    return Lhs;
}

inline EEngineShowFlags operator~(EEngineShowFlags Flag) { return static_cast<EEngineShowFlags>(~static_cast<uint64>(Flag)); }

struct FSceneViewOptions
{
    EViewModeIndex   ViewMode = EViewModeIndex::VMI_Lit;
    EEngineShowFlags ShowFlags = EEngineShowFlags::SF_Primitives | EEngineShowFlags::SF_BillboardText;
    bool             bDrawAABB = false;
    bool             bDrawGrid = true;
};