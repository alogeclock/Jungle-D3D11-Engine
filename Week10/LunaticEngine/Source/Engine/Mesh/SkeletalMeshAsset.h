#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Serialization/Archive.h"
#include "StaticMeshAsset.h"
#include "Mesh/MeshCollisionAsset.h"

#include <algorithm>

static constexpr int32 MAX_SKELETAL_MESH_UV_CHANNELS     = 4;
static constexpr int32 MAX_SKELETAL_MESH_BONE_INFLUENCES = 4;

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
//   + BoneIndices
//   + BoneWeights
//   + 현재 Bone Skinning Matrix
//   → Skinned Vertex 생성
//
// BoneIndices / BoneWeights 규칙:
// - 정점 하나는 최대 MAX_SKELETAL_MESH_BONE_INFLUENCES개의 Bone 영향을 가진다.
// - Importer에서 Weight가 큰 순서대로 상위 N개만 남긴다.
// - BoneWeights의 합은 Importer에서 1.0으로 정규화되어 있어야 한다.
// - Weight가 하나도 없는 정점은 Root Bone 100%로 보정하는 것이 안전하다.
// ============================================================================
struct FSkeletalVertex
{
    FVector  Pos;
    FVector  Normal;
    FVector4 Color;

    // UV[0]이 기본 렌더링 UV0이다.
    FVector2 UV[MAX_SKELETAL_MESH_UV_CHANNELS] = {};
    uint8    NumUVs                            = 1;

    FVector4 Tangent;

    uint16 BoneIndices[MAX_SKELETAL_MESH_BONE_INFLUENCES] = { 0, 0, 0, 0 };
    float  BoneWeights[MAX_SKELETAL_MESH_BONE_INFLUENCES] = { 0.0f, 0.0f, 0.0f, 0.0f };

    friend FArchive& operator<<(FArchive& Ar, FSkeletalVertex& V)
    {
        Ar << V.Pos;
        Ar << V.Normal;
        Ar << V.Color;

        for (int32 i = 0; i < MAX_SKELETAL_MESH_UV_CHANNELS; ++i)
        {
            Ar << V.UV[i];
        }

        Ar << V.NumUVs;
        Ar << V.Tangent;

        for (int32 i = 0; i < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++i)
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
//   SkeletalMesh reference mesh bind space 기준의 Bind Pose Transform.
//   즉 FBX world space가 아니라, importer에서 정규화한 스켈레탈 메시 기준 공간이다.
//
// - InverseBindPose
//   GlobalBindPose의 역행렬.
//   CPU/GPU Skinning Matrix 계산에 사용된다.
//
// row-vector 방식 기준 Skinning Matrix 개념:
//   SkinningMatrix = InverseBindPose * CurrentGlobalBoneTransform
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

    friend FArchive& operator<<(FArchive& Ar, FSkeleton& Skeleton)
    {
        Skeleton.Serialize(Ar);
        return Ar;
    }
};

// ============================================================================
// FSkeletalMeshLOD
//
// SkeletalMesh의 LOD 하나.
// LOD별로 독립적인 vertex/index/section/bounds를 가진다.
//
// Mesh 전체가 공유하는 것:
// - Skeleton
// - Materials
// - Animations
//
// LOD별로 달라질 수 있는 것:
// - Vertices
// - Indices
// - Sections
// - Bounds
// - NumUVChannels
// - MorphTarget delta
// ============================================================================
struct FSkeletalMeshLOD
{
    int32           SourceLODIndex = 0;
    FString         SourceLODName;
    TArray<FString> UVSetNames;

    TArray<FSkeletalVertex>    Vertices;
    TArray<uint32>             Indices;
    TArray<FStaticMeshSection> Sections;

    FVector BoundsCenter = FVector(0, 0, 0);
    FVector BoundsExtent = FVector(0, 0, 0);
    bool    bBoundsValid = false;

    uint8 NumUVChannels = 1;

    void CacheBounds()
    {
        bBoundsValid  = false;
        NumUVChannels = 1;

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

            NumUVChannels = (std::max)(NumUVChannels, V.NumUVs);
        }

        BoundsCenter = (LocalMin + LocalMax) * 0.5f;
        BoundsExtent = (LocalMax - LocalMin) * 0.5f;
        bBoundsValid = true;
    }

    void Serialize(FArchive& Ar)
    {
        Ar << SourceLODIndex;
        Ar << SourceLODName;
        Ar << UVSetNames;

        Ar << Vertices;
        Ar << Indices;
        Ar << Sections;
        Ar << NumUVChannels;

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
// Animation
// ============================================================================

struct FBoneTransformKey
{
    float TimeSeconds = 0.0f;

    FVector Translation;
    FQuat   Rotation;
    FVector Scale = FVector(1.0f, 1.0f, 1.0f);

    friend FArchive& operator<<(FArchive& Ar, FBoneTransformKey& Key)
    {
        Ar << Key.TimeSeconds;
        Ar << Key.Translation;
        Ar << Key.Rotation;
        Ar << Key.Scale;
        return Ar;
    }
};

enum class EAnimationCurveInterpolation : uint8
{
    Constant = 0,
    Linear,
    Cubic,
    Unknown
};

enum class EAnimationFloatCurveType : uint8
{
    Generic = 0,
    MorphTarget,
    BoneRaw,
};

enum class EBoneRawCurveTarget : uint8
{
    TranslationX = 0,
    TranslationY,
    TranslationZ,

    RotationX,
    RotationY,
    RotationZ,

    ScaleX,
    ScaleY,
    ScaleZ,
};

struct FFloatCurveKey
{
    float TimeSeconds = 0.0f;
    float Value       = 0.0f;

    EAnimationCurveInterpolation Interpolation = EAnimationCurveInterpolation::Unknown;

    float LeftDerivative  = 0.0f;
    float RightDerivative = 0.0f;

    bool  bLeftTangentWeighted  = false;
    bool  bRightTangentWeighted = false;
    float LeftTangentWeight     = 0.0f;
    float RightTangentWeight    = 0.0f;
    
    friend FArchive& operator<<(FArchive& Ar, FFloatCurveKey& Key)
    {
        Ar << Key.TimeSeconds;
        Ar << Key.Value;

        uint8 InterpolationValue = static_cast<uint8>(Key.Interpolation);
        Ar << InterpolationValue;

        if (Ar.IsLoading())
        {
            Key.Interpolation = static_cast<EAnimationCurveInterpolation>(InterpolationValue);
        }

        Ar << Key.LeftDerivative;
        Ar << Key.RightDerivative;

        Ar << Key.bLeftTangentWeighted;
        Ar << Key.bRightTangentWeighted;
        Ar << Key.LeftTangentWeight;
        Ar << Key.RightTangentWeight;
        
        return Ar;
    }
};

struct FAnimationFloatCurve
{
    FString Name;

    EAnimationFloatCurveType Type = EAnimationFloatCurveType::Generic;

    FString TargetName;

    FString SourceMeshNodeName;
    FString SourceBlendShapeName;
    FString SourceChannelName;

    int32   LayerIndex = 0;
    FString LayerName;
    
    TArray<FFloatCurveKey> Keys;
    
    friend FArchive& operator<<(FArchive& Ar, FAnimationFloatCurve& Curve)
    {
        Ar << Curve.Name;

        uint8 TypeValue = static_cast<uint8>(Curve.Type);
        Ar << TypeValue;

        if (Ar.IsLoading())
        {
            Curve.Type = static_cast<EAnimationFloatCurveType>(TypeValue);
        }

        Ar << Curve.TargetName;

        Ar << Curve.SourceMeshNodeName;
        Ar << Curve.SourceBlendShapeName;
        Ar << Curve.SourceChannelName;

        Ar << Curve.LayerIndex;
        Ar << Curve.LayerName;
        
        Ar << Curve.Keys;
        
        return Ar;
    }
};

struct FBoneRawFloatCurve
{
    EBoneRawCurveTarget  Target = EBoneRawCurveTarget::TranslationX;
    FAnimationFloatCurve Curve;

    friend FArchive& operator<<(FArchive& Ar, FBoneRawFloatCurve& RawCurve)
    {
        uint8 TargetValue = static_cast<uint8>(RawCurve.Target);
        Ar << TargetValue;

        if (Ar.IsLoading())
        {
            RawCurve.Target = static_cast<EBoneRawCurveTarget>(TargetValue);
        }

        Ar << RawCurve.Curve;

        return Ar;
    }
};

struct FBoneAnimationTrack
{
    int32   BoneIndex = -1;
    FString BoneName;

    TArray<FBoneTransformKey> Keys;

    TArray<FBoneRawFloatCurve> RawCurves;

    friend FArchive& operator<<(FArchive& Ar, FBoneAnimationTrack& Track)
    {
        Ar << Track.BoneIndex;
        Ar << Track.BoneName;
        Ar << Track.Keys;
        Ar << Track.RawCurves;

        return Ar;
    }
};

struct FSkeletalAnimationClip
{
    FString Name;

    float DurationSeconds = 0.0f;
    float SampleRate      = 0.0f;

    TArray<FBoneAnimationTrack>  Tracks;
    TArray<FAnimationFloatCurve> FloatCurves;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalAnimationClip& Clip)
    {
        Ar << Clip.Name;
        Ar << Clip.DurationSeconds;
        Ar << Clip.SampleRate;
        Ar << Clip.Tracks;
        Ar << Clip.FloatCurves;

        return Ar;
    }
};

// ============================================================================
// Morph Target
// ============================================================================

struct FMorphTargetDelta
{
    uint32 VertexIndex = 0;

    FVector  PositionDelta;
    FVector  NormalDelta;
    FVector4 TangentDelta;

    friend FArchive& operator<<(FArchive& Ar, FMorphTargetDelta& Delta)
    {
        Ar << Delta.VertexIndex;
        Ar << Delta.PositionDelta;
        Ar << Delta.NormalDelta;
        Ar << Delta.TangentDelta;
        return Ar;
    }
};

struct FMorphTargetShape
{
    // FBX BlendShapeChannel::GetTargetShapeFullWeights 값. 보통 100.0f가 최종 shape다.
    float FullWeight = 100.0f;

    TArray<FMorphTargetDelta> Deltas;

    friend FArchive& operator<<(FArchive& Ar, FMorphTargetShape& Shape)
    {
        Ar << Shape.FullWeight;
        Ar << Shape.Deltas;
        return Ar;
    }
};

struct FMorphTargetLOD
{
    // FBX in-between shape 전체.
    // 런타임에서 최종 shape가 필요하면 FullWeight 기준으로 선택/보간해서 사용한다.
    TArray<FMorphTargetShape> Shapes;

    friend FArchive& operator<<(FArchive& Ar, FMorphTargetLOD& LOD)
    {
        Ar << LOD.Shapes;
        return Ar;
    }
};

struct FMorphTargetSourceInfo
{
    FString SourceMeshNodeName;
    FString SourceBlendShapeName;
    FString SourceChannelName;

    friend FArchive& operator<<(FArchive& Ar, FMorphTargetSourceInfo& SourceInfo)
    {
        Ar << SourceInfo.SourceMeshNodeName;
        Ar << SourceInfo.SourceBlendShapeName;
        Ar << SourceInfo.SourceChannelName;
        return Ar;
    }
};

struct FMorphTarget
{
    FString Name;

    TArray<FMorphTargetSourceInfo> SourceInfos;
    TArray<FMorphTargetLOD>        LODModels;

    friend FArchive& operator<<(FArchive& Ar, FMorphTarget& Morph)
    {
        Ar << Morph.Name;
        Ar << Morph.SourceInfos;
        Ar << Morph.LODModels;
        return Ar;
    }
};

// ============================================================================
// Static child meshes under skeleton bones
// ============================================================================

enum class ESkeletalStaticChildImportAction : uint8
{
    MergeAsRigidPart = 0,
    KeepAsAttachedStaticMesh,
    Ignore
};

struct FSkeletalStaticChildMesh
{
    FString SourceNodeName;

    int32   ParentBoneIndex = -1;
    FString ParentBoneName;

    FMatrix LocalMatrixToParentBone;

    ESkeletalStaticChildImportAction ImportAction = ESkeletalStaticChildImportAction::MergeAsRigidPart;

    FString StaticMeshAssetPath;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalStaticChildMesh& Mesh)
    {
        Ar << Mesh.SourceNodeName;
        Ar << Mesh.ParentBoneIndex;
        Ar << Mesh.ParentBoneName;

        SkeletalMeshSerialization::SerializeMatrix(Ar, Mesh.LocalMatrixToParentBone);

        uint8 ActionValue = static_cast<uint8>(Mesh.ImportAction);
        Ar << ActionValue;

        if (Ar.IsLoading())
        {
            Mesh.ImportAction = static_cast<ESkeletalStaticChildImportAction>(ActionValue);
        }

        Ar << Mesh.StaticMeshAssetPath;

        return Ar;
    }
};

// ============================================================================
// Sockets attached to skeleton bones
// ============================================================================

struct FSkeletalSocket
{
    FString Name;
    FString SourceNodeName;

    int32   ParentBoneIndex = -1;
    FString ParentBoneName;

    FMatrix LocalMatrixToParentBone = FMatrix::Identity;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalSocket& Socket)
    {
        Ar << Socket.Name;
        Ar << Socket.SourceNodeName;
        Ar << Socket.ParentBoneIndex;
        Ar << Socket.ParentBoneName;
        SkeletalMeshSerialization::SerializeMatrix(Ar, Socket.LocalMatrixToParentBone);
        return Ar;
    }
};

// ============================================================================
// Import Summary
// ============================================================================

enum class ESkeletalImportWarningType : uint8
{
    None = 0,

    MissingNormal,
    GeneratedNormal,
    MissingTangent,
    GeneratedTangent,
    MissingUV,
    MissingSkinWeight,

    MoreThanFourInfluences,
    BoneIndexOverflow,

    UnsupportedSkinningType,
    UnsupportedClusterLinkMode,
    MissingBindPose,
    UsedClusterBindPoseFallback,
    UsedSceneTransformFallback,

    UnsupportedMorphInBetween,
    UnsupportedMorphAnimation,
    UnsupportedMaterialProperty,

    StaticChildOfBone,
    CollisionProxySkippedFromRenderLOD,

    SocketWithoutParentBone,
    DuplicateSocketName,
};

struct FSkeletalImportWarning
{
    ESkeletalImportWarningType Type = ESkeletalImportWarningType::None;
    FString                    Message;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalImportWarning& Warning)
    {
        uint8 TypeValue = static_cast<uint8>(Warning.Type);
        Ar << TypeValue;

        if (Ar.IsLoading())
        {
            Warning.Type = static_cast<ESkeletalImportWarningType>(TypeValue);
        }

        Ar << Warning.Message;

        return Ar;
    }
};

struct FSkeletalImportSummary
{
    FString SourcePath;

    int32 SourceMeshCount          = 0;
    int32 ImportedSkinnedMeshCount = 0;

    int32 BoneCount         = 0;
    int32 LODCount          = 0;
    int32 MaterialSlotCount = 0;

    int32 VertexCount             = 0;
    int32 CandidateVertexCount    = 0;
    int32 DeduplicatedVertexCount = 0;
    float DeduplicationRatio      = 0.0f;
    int32 TriangleCount           = 0;

    int32 AnimationClipCount          = 0;
    int32 AnimationTrackCount         = 0;
    int32 AnimationKeyCount           = 0;
    int32 AnimationFloatCurveCount    = 0;
    int32 AnimationFloatCurveKeyCount = 0;

    int32 MorphTargetCount      = 0;
    int32 MorphTargetShapeCount = 0;
    int32 MorphTargetDeltaCount = 0;
    
    int32 StaticChildMeshCount    = 0;
    int32 SocketCount             = 0;
    int32 CollisionProxyMeshCount = 0;

    int32 GeneratedNormalCount     = 0;
    int32 GeneratedTangentCount    = 0;
    int32 MissingUVCount           = 0;
    int32 MissingWeightVertexCount = 0;

    int32 MaxInfluenceCount            = 0;
    int32 VertexCountOverMaxInfluences = 0;
    float TotalDiscardedWeight         = 0.0f;

    float MaxBindPoseValidationError = 0.0f;

    TArray<FSkeletalImportWarning> Warnings;

    friend FArchive& operator<<(FArchive& Ar, FSkeletalImportSummary& Summary)
    {
        Ar << Summary.SourcePath;

        Ar << Summary.SourceMeshCount;
        Ar << Summary.ImportedSkinnedMeshCount;

        Ar << Summary.BoneCount;
        Ar << Summary.LODCount;
        Ar << Summary.MaterialSlotCount;
        
        Ar << Summary.VertexCount;
        Ar << Summary.CandidateVertexCount;
        Ar << Summary.DeduplicatedVertexCount;
        Ar << Summary.DeduplicationRatio;
        Ar << Summary.TriangleCount;

        Ar << Summary.AnimationClipCount;
        Ar << Summary.AnimationTrackCount;
        Ar << Summary.AnimationKeyCount;
        Ar << Summary.AnimationFloatCurveCount;
        Ar << Summary.AnimationFloatCurveKeyCount;

        Ar << Summary.MorphTargetCount;
        Ar << Summary.MorphTargetShapeCount;
        Ar << Summary.MorphTargetDeltaCount;
        
        Ar << Summary.StaticChildMeshCount;
        Ar << Summary.SocketCount;
        Ar << Summary.CollisionProxyMeshCount;

        Ar << Summary.GeneratedNormalCount;
        Ar << Summary.GeneratedTangentCount;
        Ar << Summary.MissingUVCount;
        Ar << Summary.MissingWeightVertexCount;

        Ar << Summary.MaxInfluenceCount;
        Ar << Summary.VertexCountOverMaxInfluences;
        Ar << Summary.TotalDiscardedWeight;

        Ar << Summary.MaxBindPoseValidationError;

        Ar << Summary.Warnings;

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

    TArray<FSkeletalStaticChildMesh> StaticChildMeshes;
    TArray<FImportedCollisionShape>  CollisionShapes;
    TArray<FSkeletalSocket>          Sockets;

    TArray<FSkeletalAnimationClip> Animations;
    TArray<FMorphTarget>           MorphTargets;

    FSkeletalImportSummary ImportSummary;

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
        Ar << StaticChildMeshes;
        Ar << CollisionShapes;
        Ar << Sockets;
        Ar << Animations;
        Ar << MorphTargets;
        Ar << ImportSummary;

        if (Ar.IsLoading())
        {
            Skeleton.RebuildChildren();

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