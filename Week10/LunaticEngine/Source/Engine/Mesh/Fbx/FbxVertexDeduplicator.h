#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>
#include "Mesh/StaticMeshAsset.h"
#include "Mesh/SkeletalMeshAsset.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <functional>
#include <unordered_map>

namespace FbxVertexDedupInternal
{
    // float 값을 stable hash key에 쓸 수 있는 32비트 표현으로 변환한다.
    inline uint32 FloatToStableBits(float Value)
    {
        if (Value == 0.0f)
        {
            Value = 0.0f;
        }

        uint32 Bits = 0;
        static_assert(sizeof(Bits) == sizeof(Value), "float and uint32 size mismatch");
        std::memcpy(&Bits, &Value, sizeof(float));
        return Bits;
    }

    // size_t seed에 새 hash 값을 섞는다.
    inline void HashCombineSizeT(std::size_t& Seed, std::size_t Value)
    {
        Seed ^= Value + 0x9e3779b97f4a7c15ull + (Seed << 6) + (Seed >> 2);
    }

    // uint32 값을 hash seed에 섞는다.
    inline void HashCombineUInt32(std::size_t& Seed, uint32 Value)
    {
        HashCombineSizeT(Seed, static_cast<std::size_t>(Value));
    }

    // int32 값을 hash seed에 섞는다.
    inline void HashCombineInt32(std::size_t& Seed, int32 Value)
    {
        HashCombineSizeT(Seed, static_cast<std::size_t>(static_cast<uint32>(Value)));
    }

    // uint16 값을 hash seed에 섞는다.
    inline void HashCombineUInt16(std::size_t& Seed, uint16 Value)
    {
        HashCombineSizeT(Seed, static_cast<std::size_t>(Value));
    }

    // 포인터 값을 hash seed에 섞는다.
    inline void HashCombinePointer(std::size_t& Seed, const void* Pointer)
    {
        HashCombineSizeT(Seed, std::hash<const void*> {}(Pointer));
    }

    // FVector2를 stable bit 배열로 변환한다.
    inline std::array<uint32, 2> MakeVector2Bits(const FVector2& V)
    {
        return { FloatToStableBits(V.X), FloatToStableBits(V.Y) };
    }

    // FVector를 stable bit 배열로 변환한다.
    inline std::array<uint32, 3> MakeVector3Bits(const FVector& V)
    {
        return { FloatToStableBits(V.X), FloatToStableBits(V.Y), FloatToStableBits(V.Z) };
    }

    // FVector4를 stable bit 배열로 변환한다.
    inline std::array<uint32, 4> MakeVector4Bits(const FVector4& V)
    {
        return { FloatToStableBits(V.X), FloatToStableBits(V.Y), FloatToStableBits(V.Z), FloatToStableBits(V.W) };
    }
}

struct FFbxStaticVertexDedupKey
{
    const FbxMesh* Mesh              = nullptr;
    int32          ControlPointIndex = -1;

    std::array<uint32, 3> Position = {};
    std::array<uint32, 3> Normal   = {};
    std::array<uint32, 2> UV       = {};
    std::array<uint32, 4> Color    = {};
    std::array<uint32, 4> Tangent  = {};

    // static vertex dedup key가 같은 vertex를 가리키는지 비교한다.
    bool operator==(const FFbxStaticVertexDedupKey& Other) const
    {
        return Mesh == Other.Mesh && ControlPointIndex == Other.ControlPointIndex && Position == Other.Position && Normal == Other.Normal && UV == Other.UV &&
        Color == Other.Color && Tangent == Other.Tangent;
    }
};

struct FFbxStaticVertexDedupKeyHasher
{
    // static vertex dedup key를 unordered_map용 hash 값으로 변환한다.
    std::size_t operator()(const FFbxStaticVertexDedupKey& Key) const
    {
        std::size_t Seed = 0;
        FbxVertexDedupInternal::HashCombinePointer(Seed, Key.Mesh);
        FbxVertexDedupInternal::HashCombineInt32(Seed, Key.ControlPointIndex);
        for (uint32 Value : Key.Position) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        for (uint32 Value : Key.Normal) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        for (uint32 Value : Key.UV) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        for (uint32 Value : Key.Color) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        for (uint32 Value : Key.Tangent) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        return Seed;
    }
};

class FFbxStaticVertexDeduplicator
{
public:
    // 같은 FBX corner vertex가 이미 있으면 기존 index를 반환하고 없으면 새 vertex를 추가한다.
    uint32 FindOrAdd(const FNormalVertex& Vertex, const FbxMesh* Mesh, int32 ControlPointIndex, TArray<FNormalVertex>& OutVertices, bool& bOutAddedNewVertex)
    {
        FFbxStaticVertexDedupKey Key;
        Key.Mesh              = Mesh;
        Key.ControlPointIndex = ControlPointIndex;
        Key.Position          = FbxVertexDedupInternal::MakeVector3Bits(Vertex.pos);
        Key.Normal            = FbxVertexDedupInternal::MakeVector3Bits(Vertex.normal);
        Key.UV                = FbxVertexDedupInternal::MakeVector2Bits(Vertex.tex);
        Key.Color             = FbxVertexDedupInternal::MakeVector4Bits(Vertex.color);
        Key.Tangent           = FbxVertexDedupInternal::MakeVector4Bits(Vertex.tangent);

        auto It = VertexToIndex.find(Key);
        if (It != VertexToIndex.end())
        {
            bOutAddedNewVertex = false;
            return It->second;
        }

        const uint32 NewIndex = static_cast<uint32>(OutVertices.size());
        OutVertices.push_back(Vertex);
        VertexToIndex.emplace(Key, NewIndex);
        bOutAddedNewVertex = true;
        return NewIndex;
    }

private:
    std::unordered_map<FFbxStaticVertexDedupKey, uint32, FFbxStaticVertexDedupKeyHasher> VertexToIndex;
};

struct FFbxSkeletalVertexDedupKey
{
    const FbxMesh* Mesh              = nullptr;
    int32          ControlPointIndex = -1;
    int32          MaterialIndex     = 0;

    std::array<uint32, 3>                                 Position    = {};
    std::array<uint32, 3>                                 Normal      = {};
    std::array<std::array<uint32, 2>, 4>                  UV          = {};
    std::array<uint32, 4>                                 Color       = {};
    std::array<uint32, 4>                                 Tangent     = {};
    uint8                                                 NumUV       = 0;
    std::array<uint16, MAX_SKELETAL_MESH_BONE_INFLUENCES> BoneIndices = {};
    std::array<uint32, MAX_SKELETAL_MESH_BONE_INFLUENCES> BoneWeights = {};

    // skeletal vertex dedup key가 같은 vertex를 가리키는지 비교한다.
    bool operator==(const FFbxSkeletalVertexDedupKey& Other) const
    {
        return Mesh == Other.Mesh && ControlPointIndex == Other.ControlPointIndex && MaterialIndex == Other.MaterialIndex && Position == Other.Position &&
        Normal == Other.Normal && UV == Other.UV && Color == Other.Color && Tangent == Other.Tangent && NumUV == Other.NumUV && BoneIndices == Other.BoneIndices
        && BoneWeights == Other.BoneWeights;
    }
};

struct FFbxSkeletalVertexDedupKeyHasher
{
    // skeletal vertex dedup key를 unordered_map용 hash 값으로 변환한다.
    std::size_t operator()(const FFbxSkeletalVertexDedupKey& Key) const
    {
        std::size_t Seed = 0;
        FbxVertexDedupInternal::HashCombinePointer(Seed, Key.Mesh);
        FbxVertexDedupInternal::HashCombineInt32(Seed, Key.ControlPointIndex);
        FbxVertexDedupInternal::HashCombineInt32(Seed, Key.MaterialIndex);
        for (uint32 Value : Key.Position) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        for (uint32 Value : Key.Normal) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        for (uint32 Value : Key.Color) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        for (const std::array<uint32, 2>& UV : Key.UV)
        {
            FbxVertexDedupInternal::HashCombineUInt32(Seed, UV[0]);
            FbxVertexDedupInternal::HashCombineUInt32(Seed, UV[1]);
        }
        FbxVertexDedupInternal::HashCombineInt32(Seed, static_cast<int32>(Key.NumUV));
        for (uint32 Value : Key.Tangent) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        for (uint16 Value : Key.BoneIndices) FbxVertexDedupInternal::HashCombineUInt16(Seed, Value);
        for (uint32 Value : Key.BoneWeights) FbxVertexDedupInternal::HashCombineUInt32(Seed, Value);
        return Seed;
    }
};

class FFbxSkeletalVertexDeduplicator
{
public:
    // 같은 skeletal FBX corner vertex가 이미 있으면 기존 index를 반환하고 없으면 새 vertex를 추가한다.
    uint32 FindOrAdd(
        const FSkeletalVertex&   Vertex,
        const FbxMesh*           Mesh,
        int32                    ControlPointIndex,
        int32                    MaterialIndex,
        TArray<FSkeletalVertex>& OutVertices,
        bool&                    bOutAddedNewVertex
        )
    {
        FFbxSkeletalVertexDedupKey Key;
        Key.Mesh              = Mesh;
        Key.ControlPointIndex = ControlPointIndex;
        Key.MaterialIndex     = MaterialIndex;
        Key.Position          = FbxVertexDedupInternal::MakeVector3Bits(Vertex.Pos);
        Key.Normal            = FbxVertexDedupInternal::MakeVector3Bits(Vertex.Normal);
        Key.Color             = FbxVertexDedupInternal::MakeVector4Bits(Vertex.Color);
        Key.Tangent           = FbxVertexDedupInternal::MakeVector4Bits(Vertex.Tangent);
        Key.NumUV             = Vertex.NumUVs;
        for (int32 UVIndex = 0; UVIndex < Vertex.NumUVs; ++UVIndex)
        {
            Key.UV[UVIndex] = FbxVertexDedupInternal::MakeVector2Bits(Vertex.UV[UVIndex]);
        }
        for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
        {
            Key.BoneIndices[InfluenceIndex] = Vertex.BoneIndices[InfluenceIndex];
            Key.BoneWeights[InfluenceIndex] = FbxVertexDedupInternal::FloatToStableBits(Vertex.BoneWeights[InfluenceIndex]);
        }

        auto It = VertexToIndex.find(Key);
        if (It != VertexToIndex.end())
        {
            bOutAddedNewVertex = false;
            return It->second;
        }

        const uint32 NewIndex = static_cast<uint32>(OutVertices.size());
        OutVertices.push_back(Vertex);
        VertexToIndex.emplace(Key, NewIndex);
        bOutAddedNewVertex = true;
        return NewIndex;
    }

private:
    std::unordered_map<FFbxSkeletalVertexDedupKey, uint32, FFbxSkeletalVertexDedupKeyHasher> VertexToIndex;
};
