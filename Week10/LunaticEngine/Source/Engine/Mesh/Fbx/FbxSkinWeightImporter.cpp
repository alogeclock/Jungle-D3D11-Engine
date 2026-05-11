#include "Mesh/Fbx/FbxSkinWeightImporter.h"

#include "Mesh/Fbx/FbxImportContext.h"

#include <fbxsdk.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace
{
    // 동일 bone index의 weight는 누적하고 새 bone이면 weight 항목을 추가한다.
    static void AddBoneWeight(TArray<FFbxImportedBoneWeight>& Weights, int32 BoneIndex, float Weight)
    {
        if (Weight <= 0.0f)
        {
            return;
        }

        for (FFbxImportedBoneWeight& Existing : Weights)
        {
            if (Existing.BoneIndex == BoneIndex)
            {
                Existing.Weight += Weight;
                return;
            }
        }

        FFbxImportedBoneWeight NewWeight;
        NewWeight.BoneIndex = BoneIndex;
        NewWeight.Weight    = Weight;
        Weights.push_back(NewWeight);
    }
}

void FFbxSkinWeightImporter::ExtractSkinWeightsOnly(
    FbxMesh*                                Mesh,
    const TMap<FbxNode*, int32>&            BoneNodeToIndex,
    TArray<TArray<FFbxImportedBoneWeight>>& OutControlPointWeight,
    FFbxImportContext&                      BuildContext
    )
{
    OutControlPointWeight.clear();

    if (!Mesh)
    {
        return;
    }

    const int32 ControlPointCount = Mesh->GetControlPointsCount();
    OutControlPointWeight.resize(ControlPointCount);

    const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

    for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
    {
        FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
        if (!Skin)
        {
            continue;
        }

        const FbxSkin::EType SkinningType = Skin->GetSkinningType();

        if (SkinningType != FbxSkin::eLinear && SkinningType != FbxSkin::eRigid)
        {
            BuildContext.AddWarning(
                ESkeletalImportWarningType::UnsupportedSkinningType,
                "Unsupported FBX skinning type. Imported with linear skinning fallback."
            );
        }

        const int32 ClusterCount = Skin->GetClusterCount();

        for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
        {
            FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
            if (!Cluster || !Cluster->GetLink())
            {
                continue;
            }

            const FbxCluster::ELinkMode LinkMode = Cluster->GetLinkMode();

            if (LinkMode != FbxCluster::eNormalize && LinkMode != FbxCluster::eTotalOne)
            {
                BuildContext.AddWarning(
                    ESkeletalImportWarningType::UnsupportedClusterLinkMode,
                    "Unsupported FBX cluster link mode. Additive/associate model is not fully supported."
                );
            }

            auto BoneIt = BoneNodeToIndex.find(Cluster->GetLink());
            if (BoneIt == BoneNodeToIndex.end())
            {
                continue;
            }

            const int32 BoneIndex = BoneIt->second;

            const int32*  ControlPointIndices = Cluster->GetControlPointIndices();
            const double* ControlPointWeights = Cluster->GetControlPointWeights();
            const int32   InfluenceCount      = Cluster->GetControlPointIndicesCount();

            for (int32 InfluenceIndex = 0; InfluenceIndex < InfluenceCount; ++InfluenceIndex)
            {
                const int32 ControlPointIndex = ControlPointIndices[InfluenceIndex];
                const float Weight            = static_cast<float>(ControlPointWeights[InfluenceIndex]);

                if (ControlPointIndex < 0 || ControlPointIndex >= ControlPointCount)
                {
                    continue;
                }

                AddBoneWeight(OutControlPointWeight[ControlPointIndex], BoneIndex, Weight);
            }
        }
    }
}

void FFbxSkinWeightImporter::SetRigidBoneWeight(
    int32                      BoneIndex,
    uint16                     OutBoneIndices[MAX_SKELETAL_MESH_BONE_INFLUENCES],
    float                      OutBoneWeights[MAX_SKELETAL_MESH_BONE_INFLUENCES],
    FFbxPackedBoneWeightStats& OutStats
    )
{
    OutStats                      = FFbxPackedBoneWeightStats();
    OutStats.SourceInfluenceCount = 1;

    for (int32 i = 0; i < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++i)
    {
        OutBoneIndices[i] = 0;
        OutBoneWeights[i] = 0.0f;
    }

    if (BoneIndex < 0 || BoneIndex > 65535)
    {
        OutStats.bBoneIndexOverflow = true;
        OutStats.bMissingWeight     = true;
        OutBoneIndices[0]           = 0;
        OutBoneWeights[0]           = 1.0f;
        return;
    }

    OutBoneIndices[0] = static_cast<uint16>(BoneIndex);
    OutBoneWeights[0] = 1.0f;
}

void FFbxSkinWeightImporter::PackTopBoneWeights(
    const TArray<FFbxImportedBoneWeight>& SourceWeight,
    uint16                                OutBoneIndices[MAX_SKELETAL_MESH_BONE_INFLUENCES],
    float                                 OutBoneWeights[MAX_SKELETAL_MESH_BONE_INFLUENCES],
    FFbxPackedBoneWeightStats&            OutStats
    )
{
    OutStats = FFbxPackedBoneWeightStats();

    for (int32 i = 0; i < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++i)
    {
        OutBoneIndices[i] = 0;
        OutBoneWeights[i] = 0.0f;
    }

    if (SourceWeight.empty())
    {
        OutBoneIndices[0]       = 0;
        OutBoneWeights[0]       = 1.0f;
        OutStats.bMissingWeight = true;
        return;
    }

    TArray<FFbxImportedBoneWeight> SortedWeights = SourceWeight;
    std::sort(
        SortedWeights.begin(),
        SortedWeights.end(),
        [](const FFbxImportedBoneWeight& A, const FFbxImportedBoneWeight& B)
        {
            return A.Weight > B.Weight;
        }
    );

    OutStats.SourceInfluenceCount = static_cast<int32>(SortedWeights.size());

    if (static_cast<int32>(SortedWeights.size()) > MAX_SKELETAL_MESH_BONE_INFLUENCES)
    {
        OutStats.bOverMaxInfluences = true;

        for (int32 i = MAX_SKELETAL_MESH_BONE_INFLUENCES; i < static_cast<int32>(SortedWeights.size()); ++i)
        {
            OutStats.DiscardedWeight += SortedWeights[i].Weight;
        }
    }

    const int32 Count = static_cast<int32>((std::min<std::size_t>)(static_cast<std::size_t>(MAX_SKELETAL_MESH_BONE_INFLUENCES), SortedWeights.size()));

    float Sum = 0.0f;

    for (int32 i = 0; i < Count; ++i)
    {
        if (SortedWeights[i].BoneIndex < 0 || SortedWeights[i].BoneIndex > 65535)
        {
            OutStats.bBoneIndexOverflow = true;
            continue;
        }

        OutBoneIndices[i] = static_cast<uint16>(SortedWeights[i].BoneIndex);
        OutBoneWeights[i] = SortedWeights[i].Weight;
        Sum               += OutBoneWeights[i];
    }

    if (Sum <= 1e-6f)
    {
        OutBoneIndices[0] = 0;
        OutBoneWeights[0] = 1.0f;

        for (int32 i = 1; i < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++i)
        {
            OutBoneIndices[i] = 0;
            OutBoneWeights[i] = 0.0f;
        }

        OutStats.bMissingWeight = true;
        return;
    }

    for (int32 i = 0; i < Count; ++i)
    {
        OutBoneWeights[i] /= Sum;
    }
}

void FFbxSkinWeightImporter::CommitUniqueVertexImportStats(
    FFbxImportContext&               BuildContext,
    bool                             bGeneratedNormal,
    bool                             bGeneratedTangent,
    bool                             bMissingUV,
    const FFbxPackedBoneWeightStats& BoneWeightStats
    )
{
    BuildContext.Summary.VertexCount++;

    if (bGeneratedNormal)
    {
        BuildContext.Summary.GeneratedNormalCount++;
    }

    if (bGeneratedTangent)
    {
        BuildContext.Summary.GeneratedTangentCount++;
    }

    if (bMissingUV)
    {
        BuildContext.Summary.MissingUVCount++;
    }

    if (BoneWeightStats.bMissingWeight)
    {
        BuildContext.Summary.MissingWeightVertexCount++;
    }

    BuildContext.Summary.MaxInfluenceCount = (std::max)(BuildContext.Summary.MaxInfluenceCount, BoneWeightStats.SourceInfluenceCount);

    if (BoneWeightStats.bOverMaxInfluences)
    {
        BuildContext.Summary.VertexCountOverMaxInfluences++;
        BuildContext.Summary.TotalDiscardedWeight += BoneWeightStats.DiscardedWeight;
    }

    if (BoneWeightStats.bBoneIndexOverflow)
    {
        BuildContext.AddWarning(ESkeletalImportWarningType::BoneIndexOverflow, "Bone index is outside uint16 range.");
    }
}
