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
#include "Mesh/Fbx/FbxCollisionImporter.h"
#include "Mesh/Fbx/FbxSocketImporter.h"

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
    if (!FFbxSkeletonImporter::BuildSkeletonFromSkinClusters(SkinnedMeshNodes, MeshNodes, OutMesh.Skeleton, BoneNodeToIndex, &BuildContext))
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
    OutMesh.LODModels.clear();
    OutMesh.StaticChildMeshes.clear();
    OutMesh.Sockets.clear();
    OutMesh.CollisionShapes.clear();
    OutMesh.Animations.clear();
    OutMesh.MorphTargets.clear();

    for (const FFbxSkeletalImportMeshNode& ImportNode : ImportMeshNodes)
    {
        if (!ImportNode.MeshNode)
        {
            continue;
        }

        if (ImportNode.Kind == EFbxSkeletalImportMeshKind::CollisionProxy)
        {
            FString ParentBoneName;

            if (ImportNode.ParentBoneIndex >= 0 && ImportNode.ParentBoneIndex < static_cast<int32>(OutMesh.Skeleton.Bones.size()))
            {
                ParentBoneName = OutMesh.Skeleton.Bones[ImportNode.ParentBoneIndex].Name;
            }

            FImportedCollisionShape Shape;

            if (FFbxCollisionImporter::ImportCollisionShape(
                ImportNode.MeshNode,
                ImportNode.LocalMatrixToParentBone,
                ImportNode.ParentBoneIndex,
                ParentBoneName,
                Shape
            ))
            {
                OutMesh.CollisionShapes.push_back(Shape);
            }
            
            BuildContext.Summary.CollisionProxyMeshCount++;

            BuildContext.AddWarningOnce(
                ESkeletalImportWarningType::CollisionProxySkippedFromRenderLOD,
                "Collision proxy imported and skipped from render LOD: " + ImportNode.SourceNodeName
            );

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

    TArray<bool> AppliedPoseBindMask;
    AppliedPoseBindMask.resize(OutMesh.Skeleton.Bones.size(), false);

    TArray<bool> AppliedClusterBindMask;
    AppliedClusterBindMask.resize(OutMesh.Skeleton.Bones.size(), false);

    FFbxSkeletonImporter::ApplyBindPoseFromFbxPose(
        Scene,
        BoneNodeToIndex,
        ReferenceMeshBindInverse,
        OutMesh.Skeleton,
        AppliedPoseBindMask,
        BuildContext
    );

    // Reference LOD의 cluster link matrix를 weighted bone의 최우선 bind source로 사용한다.
    FFbxSkeletonImporter::ApplyBindPoseFromSkinClusters(
        ReferenceLODNodes,
        BoneNodeToIndex,
        ReferenceMeshBindInverse,
        OutMesh.Skeleton,
        &AppliedClusterBindMask,
        true
    );

    // 다른 LOD의 cluster는 reference LOD에 없던 bone만 보완한다.
    FFbxSkeletonImporter::ApplyBindPoseFromSkinClusters(
        SkinnedMeshNodes,
        BoneNodeToIndex,
        ReferenceMeshBindInverse,
        OutMesh.Skeleton,
        &AppliedClusterBindMask,
        false
    );

    // cluster가 없는 helper/end/IK/attachment bone은 pose global을 그대로 믿지 않는다.
    FFbxSkeletonImporter::FinalizeNonClusterBoneBindPose(
        Scene,
        BoneNodeToIndex,
        ReferenceMeshBindInverse,
        AppliedClusterBindMask,
        OutMesh.Skeleton,
        BuildContext
    );

    FFbxSkeletonImporter::RecomputeLocalBindPose(OutMesh.Skeleton);

    FFbxSocketImporter::ImportSockets(
        Scene,
        BoneNodeToIndex,
        ReferenceMeshBindInverse,
        OutMesh.Skeleton,
        OutMesh.Sockets,
        BuildContext
    );

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

    BuildContext.Summary.BoneCount         = static_cast<int32>(OutMesh.Skeleton.Bones.size());
    BuildContext.Summary.LODCount          = static_cast<int32>(OutMesh.LODModels.size());
    BuildContext.Summary.MaterialSlotCount = static_cast<int32>(OutMaterials.size());

    BuildContext.Summary.AnimationClipCount          = static_cast<int32>(OutMesh.Animations.size());
    BuildContext.Summary.AnimationTrackCount         = 0;
    BuildContext.Summary.AnimationKeyCount           = 0;
    BuildContext.Summary.AnimationFloatCurveCount    = 0;
    BuildContext.Summary.AnimationFloatCurveKeyCount = 0;

    for (const FSkeletalAnimationClip& Clip : OutMesh.Animations)
    {
        BuildContext.Summary.AnimationTrackCount += static_cast<int32>(Clip.Tracks.size());

        for (const FBoneAnimationTrack& Track : Clip.Tracks)
        {
            BuildContext.Summary.AnimationKeyCount        += static_cast<int32>(Track.Keys.size());
            BuildContext.Summary.AnimationFloatCurveCount += static_cast<int32>(Track.RawCurves.size());

            for (const FBoneRawFloatCurve& RawCurve : Track.RawCurves)
            {
                BuildContext.Summary.AnimationFloatCurveKeyCount += static_cast<int32>(RawCurve.Curve.Keys.size());
            }
        }

        BuildContext.Summary.AnimationFloatCurveCount += static_cast<int32>(Clip.FloatCurves.size());

        for (const FAnimationFloatCurve& Curve : Clip.FloatCurves)
        {
            BuildContext.Summary.AnimationFloatCurveKeyCount += static_cast<int32>(Curve.Keys.size());
        }
    }

    BuildContext.Summary.MorphTargetCount      = static_cast<int32>(OutMesh.MorphTargets.size());
    BuildContext.Summary.MorphTargetShapeCount = 0;
    BuildContext.Summary.MorphTargetDeltaCount = 0;

    for (const FMorphTarget& Morph : OutMesh.MorphTargets)
    {
        for (const FMorphTargetLOD& LOD : Morph.LODModels)
        {
            BuildContext.Summary.MorphTargetShapeCount += static_cast<int32>(LOD.Shapes.size());

            for (const FMorphTargetShape& Shape : LOD.Shapes)
            {
                BuildContext.Summary.MorphTargetDeltaCount += static_cast<int32>(Shape.Deltas.size());
            }
        }
    }

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
