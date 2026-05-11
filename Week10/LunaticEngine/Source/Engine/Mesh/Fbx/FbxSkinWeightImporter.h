#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>
#include "Mesh/SkeletalMeshAsset.h"

struct FFbxImportContext;

struct FFbxImportedBoneWeight
{
    int32 BoneIndex = 0;
    float Weight    = 0.0f;
};

struct FFbxPackedBoneWeightStats
{
    bool  bMissingWeight       = false;
    bool  bOverMaxInfluences   = false;
    bool  bBoneIndexOverflow   = false;
    int32 SourceInfluenceCount = 0;
    float DiscardedWeight      = 0.0f;
};

class FFbxSkinWeightImporter
{
public:
    // FBX skin cluster weight를 control point별 bone weight 배열로 추출한다.
    static void ExtractSkinWeightsOnly(
        FbxMesh*                                Mesh,
        const TMap<FbxNode*, int32>&            BoneNodeToIndex,
        TArray<TArray<FFbxImportedBoneWeight>>& OutControlPointWeights,
        FFbxImportContext&                      BuildContext
        );

    // rigid attached mesh vertex에 단일 bone weight를 설정한다.
    static void SetRigidBoneWeight(
        int32                      BoneIndex,
        uint16                     OutBoneIndices[MAX_SKELETAL_MESH_BONE_INFLUENCES],
        float                      OutBoneWeights[MAX_SKELETAL_MESH_BONE_INFLUENCES],
        FFbxPackedBoneWeightStats& OutStats
        );

    // source weight 배열에서 상위 influence를 골라 4개 이하로 정규화해 pack한다.
    static void PackTopBoneWeights(
        const TArray<FFbxImportedBoneWeight>& Weights,
        uint16                                OutBoneIndices[MAX_SKELETAL_MESH_BONE_INFLUENCES],
        float                                 OutBoneWeights[MAX_SKELETAL_MESH_BONE_INFLUENCES],
        FFbxPackedBoneWeightStats&            OutStats
        );

    // unique vertex가 추가됐을 때 import summary와 warning 정보를 갱신한다.
    static void CommitUniqueVertexImportStats(
        FFbxImportContext&               BuildContext,
        bool                             bGeneratedNormal,
        bool                             bGeneratedTangent,
        bool                             bMissingUV,
        const FFbxPackedBoneWeightStats& BoneWeightStats
        );
};
