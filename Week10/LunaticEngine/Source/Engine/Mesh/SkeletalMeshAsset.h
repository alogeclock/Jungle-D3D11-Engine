#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Serialization/Archive.h"
#include "StaticMeshAsset.h"

#include <algorithm>

namespace SkeletalMeshSerialization
{
    inline void SerializeMatrix(FArchive& Ar, FMatrix& Matrix)
    {
        Ar.Serialize(&Matrix.M[0][0], sizeof(Matrix.M));
    }
}

// ============================================================================
// FSkeletalVertex
//
// 스켈레탈 메시의 원본 정점.
// StaticMesh의 FNormalVertex와 비슷하지만, BoneIndices / BoneWeights가 추가된다.
//
// CPU Skinning 시 이 정점은 다음 과정을 거친다.
//   원본 위치/노멀/탄젠트
//   + BoneIndices[4]
//   + BoneWeights[4]
//   + 현재 Bone Skinning Matrix
//   → Skinned Vertex 생성
//
// BoneIndices / BoneWeights 규칙:
// - 정점 하나는 최대 4개의 Bone 영향을 가진다.
// - Importer에서 Weight가 큰 순서대로 상위 4개만 남기는 것을 권장한다.
// - BoneWeights의 합은 Importer에서 1.0으로 정규화되어 있어야 한다.
// - Weight가 하나도 없는 정점은 Root Bone 100%로 보정하는 것이 안전하다.
// ============================================================================
struct FSkeletalVertex
{
    FVector  Pos;
    FVector  Normal;
    FVector4 Color;
    FVector2 UV;
    FVector4 Tangent;

    uint16 BoneIndices[4] = { 0, 0, 0, 0 };
    float  BoneWeights[4] = { 0, 0, 0, 0 };

    friend FArchive& operator<<(FArchive& Ar, FSkeletalVertex& V)
    {
        Ar << V.Pos;
        Ar << V.Normal;
        Ar << V.Color;
        Ar << V.UV;
        Ar << V.Tangent;

        for (int32 i = 0; i < 4; ++i)
        {
            Ar << V.BoneIndices[i];
            Ar << V.BoneWeights[i];
        }

        return Ar;
    }
};

// ============================================================================
// FBoneInfo
//
// Skeleton을 구성하는 Bone 하나의 정보.
// Bone은 실제 렌더링되는 메시가 아니라, 정점을 변형하기 위한 Transform 노드다.
//
// - LocalBindPose
//   부모 Bone 기준의 Bind Pose Transform.
//
// - GlobalBindPose
//   메시 로컬 공간 기준의 Bind Pose Transform.
//   즉 Root부터 이 Bone까지 계층 Transform을 누적한 결과.
//
// - InverseBindPose
//   GlobalBindPose의 역행렬.
//   Skinning Matrix 계산에 사용된다.
//
// 일반적인 Skinning Matrix 개념:
// SkinningMatrix = InverseBindPose * CurrentGlobalBoneTransform
// ============================================================================
struct FBoneInfo
{
    FString Name;
    int32   ParentIndex = -1;

    FMatrix LocalBindPose;
    FMatrix GlobalBindPose;
    FMatrix InverseBindPose;

    TArray<int32> Children;

    friend FArchive& operator<<(FArchive& Ar, FBoneInfo& B)
    {
        Ar << B.Name;
        Ar << B.ParentIndex;

        SkeletalMeshSerialization::SerializeMatrix(Ar, B.LocalBindPose);
        SkeletalMeshSerialization::SerializeMatrix(Ar, B.GlobalBindPose);
        SkeletalMeshSerialization::SerializeMatrix(Ar, B.InverseBindPose);

        return Ar;
    }
};

// ============================================================================
// FSkeleton
//
// SkeletalMesh가 사용하는 Bone 계층 전체.
// Mesh 전체가 하나의 Skeleton을 공유하고, LOD들은 이 Skeleton을 참조한다.
//
// - Bone 배열 보관
// - ParentIndex 기반 Children 재구성
// - Bone 이름으로 Bone Index 검색
// ============================================================================
struct FSkeleton
{
    TArray<FBoneInfo> Bones;

    void RebuildChildren()
    {
        for (FBoneInfo& Bone : Bones)
        {
            Bone.Children.clear();
        }

        for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
        {
            const int32 ParentIndex = Bones[BoneIndex].ParentIndex;

            if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(Bones.size()))
            {
                Bones[ParentIndex].Children.push_back(BoneIndex);
            }
        }
    }

    int32 FindBoneIndexByName(const FString& BoneName) const
    {
        for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
        {
            if (Bones[BoneIndex].Name == BoneName)
            {
                return BoneIndex;
            }
        }

        return -1;
    }

    void Serialize(FArchive& Ar)
    {
        Ar << Bones;

        if (Ar.IsLoading())
        {
            RebuildChildren();
        }
    }
};

// ============================================================================
// FSkeletalMeshLOD
//
// SkeletalMesh의 LOD 하나.
// LOD... 해야할까?
//
// LOD별로 달라질 수 있는 것:
// - Vertices
// - Indices
// - Sections
// - Bounds
//
// LOD와 무관하게 Mesh 전체가 공유하는 것:
// - Skeleton
// - Materials
// ============================================================================
struct FSkeletalMeshLOD
{
    TArray<FSkeletalVertex>    Vertices;
    TArray<uint32>             Indices;
    TArray<FStaticMeshSection> Sections;

    FVector BoundsCenter = FVector(0, 0, 0);
    FVector BoundsExtent = FVector(0, 0, 0);
    bool    bBoundsValid = false;

    void CacheBounds()
    {
        bBoundsValid = false;

        if (Vertices.empty())
        {
            return;
        }

        FVector LocalMin = Vertices[0].Pos;
        FVector LocalMax = Vertices[0].Pos;

        for (const FSkeletalVertex& V : Vertices)
        {
            LocalMin.X = (std::min)(LocalMin.X, V.Pos.X);
            LocalMin.Y = (std::min)(LocalMin.Y, V.Pos.Y);
            LocalMin.Z = (std::min)(LocalMin.Z, V.Pos.Z);

            LocalMax.X = (std::max)(LocalMax.X, V.Pos.X);
            LocalMax.Y = (std::max)(LocalMax.Y, V.Pos.Y);
            LocalMax.Z = (std::max)(LocalMax.Z, V.Pos.Z);
        }

        BoundsCenter = (LocalMin + LocalMax) * 0.5f;
        BoundsExtent = (LocalMax - LocalMin) * 0.5f;
        bBoundsValid = true;
    }

    void Serialize(FArchive& Ar)
    {
        Ar << Vertices;
        Ar << Indices;
        Ar << Sections;

        if (Ar.IsLoading())
        {
            CacheBounds();
        }
    }

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshLOD& LOD)
    {
        LOD.Serialize(Ar);
        return Ar;
    }
};

// ============================================================================
// FSkeletalMesh
//
// Bake 파일 또는 FBX Importer가 만들어내는 최종 SkeletalMesh 원본 리소스.
// UObject가 아니라 순수 데이터 구조다.
//
// USkeletalMesh는 이 FSkeletalMesh를 포인터로 들고 있는 에셋 래퍼다.
// ============================================================================
struct FSkeletalMesh
{
    FString PathFileName;

    FSkeleton Skeleton;

    TArray<FSkeletalMeshLOD> LODModels;

    FSkeletalMeshLOD* GetLOD(int32 LODIndex)
    {
        if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODModels.size()))
        {
            return nullptr;
        }

        return &LODModels[LODIndex];
    }

    const FSkeletalMeshLOD* GetLOD(int32 LODIndex) const
    {
        if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODModels.size()))
        {
            return nullptr;
        }

        return &LODModels[LODIndex];
    }

    void Serialize(FArchive& Ar)
    {
        Ar << PathFileName;

        Skeleton.Serialize(Ar);

        Ar << LODModels;

        if (Ar.IsLoading())
        {
            for (FSkeletalMeshLOD& LOD : LODModels)
            {
                LOD.CacheBounds();
            }
        }
    }

    friend FArchive& operator<<(FArchive& Ar, FSkeletalMesh& Mesh)
    {
        Mesh.Serialize(Ar);
        return Ar;
    }
};
