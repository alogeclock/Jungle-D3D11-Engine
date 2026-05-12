#include "Mesh/Fbx/FbxSkeletalMeshImporter.h"

#include "Mesh/Fbx/FbxAnimationImporter.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxImportValidation.h"
#include "Mesh/Fbx/FbxMorphTargetImporter.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxSkeletalMeshClassifier.h"
#include "Mesh/Fbx/FbxSkeletalMeshLODBuilder.h"
#include "Mesh/Fbx/FbxSkeletonImporter.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/StaticMeshAsset.h"

#include <fbxsdk.h>

#include <algorithm>
#include <utility>

// FBX scene에서 skeletal mesh, skeleton, LOD, morph target, animation을 import한다.
bool FFbxSkeletalMeshImporter::Import(
    FbxScene*                Scene,
    const FString&           SourcePath,
    FSkeletalMesh&           OutMesh,
    TArray<FStaticMaterial>& OutMaterials,
    FFbxImportContext&       BuildContext
    )
{
    if (!Scene || !Scene->GetRootNode())
    {
        return false;
    }

    OutMesh.PathFileName = SourcePath;

    TArray<FbxNode*> MeshNodes;
    FFbxSceneQuery::CollectMeshNodes(Scene->GetRootNode(), MeshNodes);
    BuildContext.Summary.SourceMeshCount = static_cast<int32>(MeshNodes.size());

    TArray<FbxNode*> SkinnedMeshNodes;
    for (FbxNode* MeshNode : MeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (Mesh && FFbxSceneQuery::MeshHasSkin(Mesh))
        {
            SkinnedMeshNodes.push_back(MeshNode);
        }
    }

    BuildContext.Summary.ImportedSkinnedMeshCount = static_cast<int32>(SkinnedMeshNodes.size());

    if (SkinnedMeshNodes.empty())
    {
        return false;
    }

    TMap<FbxNode*, int32> BoneNodeToIndex;
    if (!FFbxSkeletonImporter::BuildSkeletonFromSkinClusters(SkinnedMeshNodes, MeshNodes, OutMesh.Skeleton, BoneNodeToIndex))
    {
        return false;
    }

    TArray<FFbxSkeletalImportMeshNode> ImportMeshNodes;
    FFbxSkeletalMeshClassifier::Classify(MeshNodes, BoneNodeToIndex, ImportMeshNodes);

    TMap<int32, TArray<FbxNode*>> SkinnedMeshNodesByLOD;
    for (FbxNode* MeshNode : SkinnedMeshNodes)
    {
        const int32 LODIndex = FFbxSceneQuery::GetSkeletalMeshLODIndex(MeshNode);
        SkinnedMeshNodesByLOD[LODIndex].push_back(MeshNode);
    }

    TMap<int32, TArray<FFbxSkeletalImportMeshNode>> MeshNodesByLOD;
    OutMesh.StaticChildMeshes.clear();

    for (const FFbxSkeletalImportMeshNode& ImportNode : ImportMeshNodes)
    {
        if (!ImportNode.MeshNode)
        {
            continue;
        }

        if (ImportNode.Kind == EFbxSkeletalImportMeshKind::CollisionProxy)
        {
            BuildContext.Summary.CollisionProxyMeshCount++;
            continue;
        }

        if (ImportNode.Kind == EFbxSkeletalImportMeshKind::StaticChildOfBone)
        {
            FSkeletalStaticChildMesh StaticChild;
            StaticChild.SourceNodeName  = ImportNode.SourceNodeName;
            StaticChild.ParentBoneIndex = ImportNode.ParentBoneIndex;
            StaticChild.ParentBoneName  = (ImportNode.ParentBoneIndex >= 0 && ImportNode.ParentBoneIndex < static_cast<int32>(OutMesh.Skeleton.Bones.size()))
            ? OutMesh.Skeleton.Bones[ImportNode.ParentBoneIndex].Name : FString();
            StaticChild.LocalMatrixToParentBone = ImportNode.LocalMatrixToParentBone;
            StaticChild.ImportAction            = ImportNode.StaticChildAction;

            OutMesh.StaticChildMeshes.push_back(StaticChild);
            BuildContext.Summary.StaticChildMeshCount++;

            if (ImportNode.StaticChildAction != ESkeletalStaticChildImportAction::MergeAsRigidPart)
            {
                continue;
            }
        }

        if (ImportNode.Kind == EFbxSkeletalImportMeshKind::Loose || ImportNode.Kind == EFbxSkeletalImportMeshKind::Ignored || ImportNode.Kind ==
            EFbxSkeletalImportMeshKind::CollisionProxy)
        {
            continue;
        }

        const int32 LODIndex = FFbxSceneQuery::GetSkeletalMeshLODIndex(ImportNode.MeshNode);
        MeshNodesByLOD[LODIndex].push_back(ImportNode);
    }
    
    TArray<int32> SortedLODIndices;
    for (const auto& Pair : MeshNodesByLOD)
    {
        SortedLODIndices.push_back(Pair.first);
    }

    std::sort(SortedLODIndices.begin(), SortedLODIndices.end());

    if (SortedLODIndices.empty())
    {
        return false;
    }

    TArray<int32> SortedSkinnedLODIndices;
    for (const auto& Pair : SkinnedMeshNodesByLOD)
    {
        SortedSkinnedLODIndices.push_back(Pair.first);
    }

    std::sort(SortedSkinnedLODIndices.begin(), SortedSkinnedLODIndices.end());

    if (SortedSkinnedLODIndices.empty())
    {
        return false;
    }

    const int32             ReferenceLODIndex = SortedSkinnedLODIndices[0];
    const TArray<FbxNode*>& ReferenceLODNodes = SkinnedMeshNodesByLOD[ReferenceLODIndex];

    FMatrix ReferenceMeshBind;
    if (!FFbxSkeletonImporter::TryGetReferenceMeshBindMatrix(Scene, ReferenceLODNodes, ReferenceMeshBind))
    {
        return false;
    }

    const FMatrix ReferenceMeshBindInverse = ReferenceMeshBind.GetInverse();

    FFbxSkeletonImporter::InitializeBoneBindPoseFromSceneNodes(BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton);

    TArray<bool> AppliedClusterBindPose;
    AppliedClusterBindPose.resize(OutMesh.Skeleton.Bones.size());

    FFbxSkeletonImporter::ApplyBindPoseFromFbxPose(Scene, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, AppliedClusterBindPose, BuildContext);

    FFbxSkeletonImporter::ApplyBindPoseFromSkinClusters(
        ReferenceLODNodes,
        BoneNodeToIndex,
        ReferenceMeshBindInverse,
        OutMesh.Skeleton,
        &AppliedClusterBindPose
    );
    FFbxSkeletonImporter::ApplyBindPoseFromSkinClusters(SkinnedMeshNodes, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, &AppliedClusterBindPose);

    FFbxSkeletonImporter::RecomputeLocalBindPose(OutMesh.Skeleton);

    TArray<TArray<FFbxImportedMorphSourceVertex>> MorphSourcesByLOD;

    for (int32 LODIndex : SortedLODIndices)
    {
        FSkeletalMeshLOD                      NewLOD;
        TArray<FFbxImportedMorphSourceVertex> MorphSources;

        if (!FFbxSkeletalMeshLODBuilder::Build(
            Scene,
            SourcePath,
            MeshNodesByLOD[LODIndex],
            BoneNodeToIndex,
            ReferenceMeshBindInverse,
            OutMesh.Skeleton,
            OutMaterials,
            NewLOD,
            &MorphSources,
            BuildContext
        ))
        {
            continue;
        }

        NewLOD.SourceLODIndex = LODIndex;
        NewLOD.SourceLODName  = FString("LOD") + std::to_string(LODIndex);

        OutMesh.LODModels.push_back(NewLOD);
        MorphSourcesByLOD.push_back(std::move(MorphSources));
    }

    if (OutMesh.LODModels.empty())
    {
        return false;
    }

    FFbxMorphTargetImporter::ImportMorphTargets(MorphSourcesByLOD, OutMesh.MorphTargets, BuildContext);

    FFbxAnimationImporter::ImportAnimations(Scene, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, OutMesh.Animations);

    float MaxBindPoseError = 0.0f;
    for (const FSkeletalMeshLOD& LOD : OutMesh.LODModels)
    {
        MaxBindPoseError = (std::max)(MaxBindPoseError, FFbxImportValidation::ValidateBindPoseSkinningError(LOD, OutMesh.Skeleton));
    }

    BuildContext.Summary.MaxBindPoseValidationError = MaxBindPoseError;

    if (MaxBindPoseError > 0.001f)
    {
        BuildContext.AddWarning(ESkeletalImportWarningType::MissingBindPose, "Bind pose validation error is larger than tolerance.");
    }

    BuildContext.Summary.BoneCount          = static_cast<int32>(OutMesh.Skeleton.Bones.size());
    BuildContext.Summary.LODCount           = static_cast<int32>(OutMesh.LODModels.size());
    BuildContext.Summary.MaterialSlotCount  = static_cast<int32>(OutMaterials.size());
    BuildContext.Summary.AnimationClipCount = static_cast<int32>(OutMesh.Animations.size());
    BuildContext.Summary.MorphTargetCount   = static_cast<int32>(OutMesh.MorphTargets.size());

    if (BuildContext.Summary.CandidateVertexCount > 0)
    {
        BuildContext.Summary.DeduplicationRatio = static_cast<float>(BuildContext.Summary.DeduplicatedVertexCount) / static_cast<float>(BuildContext.Summary.
            CandidateVertexCount);
    }
    else
    {
        BuildContext.Summary.DeduplicationRatio = 0.0f;
    }

    OutMesh.ImportSummary = BuildContext.Summary;

    return true;
}
