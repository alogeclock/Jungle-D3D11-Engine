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
#include "Mesh/Fbx/FbxStaticChildMeshImporter.h"
#include "Mesh/Fbx/FbxMetadataImporter.h"
#include "Mesh/Fbx/FbxSceneHierarchyImporter.h"
#include "Mesh/Fbx/FbxTransformUtils.h"
#include "Math/Transform.h"

#include <fbxsdk.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    FMatrix TransformDirectionMatrix(const FMatrix& Matrix)
    {
        return FFbxTransformUtils::RemoveTranslationFromMatrix(Matrix);
    }

    void FlipLODIndexWinding(FSkeletalMeshLOD& LOD)
    {
        for (size_t Index = 0; Index + 2 < LOD.Indices.size(); Index += 3)
        {
            std::swap(LOD.Indices[Index + 1], LOD.Indices[Index + 2]);
        }
    }

    void TransformLODToEngineAssetSpace(FSkeletalMeshLOD& LOD, const FMatrix& AssetCorrection)
    {
        const FMatrix DirectionCorrection = TransformDirectionMatrix(AssetCorrection);
        const FMatrix NormalCorrection    = DirectionCorrection.GetInverse().GetTransposed();
        const bool    bReverseWinding     = FFbxTransformUtils::Determinant3x3(DirectionCorrection) < 0.0f;

        for (FSkeletalVertex& Vertex : LOD.Vertices)
        {
            Vertex.Pos    = FFbxTransformUtils::TransformPositionByMatrix(Vertex.Pos, AssetCorrection);
            Vertex.Normal = FFbxTransformUtils::TransformNormalByMatrix(Vertex.Normal, NormalCorrection);

            const FVector Tangent = FFbxTransformUtils::TransformTangentByMatrix(
                FVector(Vertex.Tangent.X, Vertex.Tangent.Y, Vertex.Tangent.Z),
                DirectionCorrection,
                Vertex.Normal
            );
            Vertex.Tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, bReverseWinding ? -Vertex.Tangent.W : Vertex.Tangent.W);
        }

        if (bReverseWinding)
        {
            FlipLODIndexWinding(LOD);
        }

        LOD.CacheBounds();
    }

    void TransformMorphTargetsToEngineAssetSpace(TArray<FMorphTarget>& MorphTargets, const FMatrix& AssetCorrection)
    {
        const FMatrix DirectionCorrection = TransformDirectionMatrix(AssetCorrection);
        const FMatrix NormalCorrection    = DirectionCorrection.GetInverse().GetTransposed();

        for (FMorphTarget& MorphTarget : MorphTargets)
        {
            for (FMorphTargetLOD& MorphLOD : MorphTarget.LODModels)
            {
                for (FMorphTargetShape& Shape : MorphLOD.Shapes)
                {
                    for (FMorphTargetDelta& Delta : Shape.Deltas)
                    {
                        Delta.PositionDelta = FFbxTransformUtils::TransformVectorNoNormalizeByMatrix(Delta.PositionDelta, DirectionCorrection);
                        Delta.NormalDelta   = FFbxTransformUtils::TransformNormalByMatrix(Delta.NormalDelta, NormalCorrection);

                        const FVector TangentDelta = FFbxTransformUtils::TransformVectorNoNormalizeByMatrix(
                            FVector(Delta.TangentDelta.X, Delta.TangentDelta.Y, Delta.TangentDelta.Z),
                            DirectionCorrection
                        );
                        Delta.TangentDelta = FVector4(TangentDelta.X, TangentDelta.Y, TangentDelta.Z, Delta.TangentDelta.W);
                    }
                }
            }
        }
    }

    void TransformSkeletonToEngineAssetSpace(FSkeleton& Skeleton, const FMatrix& AssetCorrection)
    {
        for (FBoneInfo& Bone : Skeleton.Bones)
        {
            Bone.GlobalBindPose  = Bone.GlobalBindPose * AssetCorrection;
            Bone.InverseBindPose = Bone.GlobalBindPose.GetInverse();
        }

        FFbxSkeletonImporter::RecomputeLocalBindPose(Skeleton);
    }

    void TransformAnimationRootKeysToEngineAssetSpace(TArray<FSkeletalAnimationClip>& Animations, const FSkeleton& Skeleton, const FMatrix& AssetCorrection)
    {
        for (FSkeletalAnimationClip& Clip : Animations)
        {
            for (FBoneAnimationTrack& Track : Clip.Tracks)
            {
                if (Track.BoneIndex < 0 || Track.BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
                {
                    continue;
                }

                if (Skeleton.Bones[Track.BoneIndex].ParentIndex >= 0)
                {
                    continue;
                }

                for (FBoneTransformKey& Key : Track.Keys)
                {
                    const FMatrix LocalMatrix = FTransform(Key.Translation, Key.Rotation, Key.Scale).ToMatrix();
                    Key                       = FFbxTransformUtils::MakeBoneTransformKeyFromEngineMatrix(Key.TimeSeconds, LocalMatrix * AssetCorrection);
                }
            }
        }
    }

    void TransformSceneNodesToEngineAssetSpace(TArray<FFbxImportedSceneNode>& SceneNodes, const FMatrix& AssetCorrection)
    {
        for (FFbxImportedSceneNode& SceneNode : SceneNodes)
        {
            SceneNode.GlobalMatrix         = SceneNode.GlobalMatrix * AssetCorrection;
            SceneNode.GlobalGeometryMatrix = SceneNode.GlobalGeometryMatrix * AssetCorrection;
        }
    }

    void TransformSplitStaticMeshesToEngineAssetSpace(TArray<FFbxSplitStaticMeshReference>& SplitStaticMeshes, const FMatrix& AssetCorrection)
    {
        for (FFbxSplitStaticMeshReference& SplitRef : SplitStaticMeshes)
        {
            SplitRef.GlobalMatrix = SplitRef.GlobalMatrix * AssetCorrection;
        }
    }

    void BakeNormalizedSkeletalSpaceToEngineAssetSpace(FSkeletalMesh& Mesh, const FMatrix& AssetCorrection)
    {
        for (FSkeletalMeshLOD& LOD : Mesh.LODModels)
        {
            TransformLODToEngineAssetSpace(LOD, AssetCorrection);
        }

        TransformMorphTargetsToEngineAssetSpace(Mesh.MorphTargets, AssetCorrection);
        TransformSkeletonToEngineAssetSpace(Mesh.Skeleton, AssetCorrection);
        TransformAnimationRootKeysToEngineAssetSpace(Mesh.Animations, Mesh.Skeleton, AssetCorrection);
        TransformSplitStaticMeshesToEngineAssetSpace(Mesh.SplitStaticMeshes, AssetCorrection);
    }

    void SanitizeLODInfluencesForValidation(FSkeletalMeshLOD& LOD, int32 BoneCount)
    {
        for (FSkeletalVertex& Vertex : LOD.Vertices)
        {
            float TotalWeight = 0.0f;
            for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
            {
                const bool bValidBone = BoneCount > 0 && Vertex.BoneIndices[InfluenceIndex] < static_cast<uint16>(BoneCount);
                const bool bValidWeight = std::isfinite(Vertex.BoneWeights[InfluenceIndex]) && Vertex.BoneWeights[InfluenceIndex] > 0.0f;
                if (!bValidBone || !bValidWeight)
                {
                    Vertex.BoneIndices[InfluenceIndex] = 0;
                    Vertex.BoneWeights[InfluenceIndex] = 0.0f;
                    continue;
                }

                TotalWeight += Vertex.BoneWeights[InfluenceIndex];
            }

            if (TotalWeight <= 1e-6f)
            {
                for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
                {
                    Vertex.BoneIndices[InfluenceIndex] = 0;
                    Vertex.BoneWeights[InfluenceIndex] = 0.0f;
                }

                if (BoneCount > 0)
                {
                    Vertex.BoneWeights[0] = 1.0f;
                }
                continue;
            }

            const float InvTotalWeight = 1.0f / TotalWeight;
            for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
            {
                Vertex.BoneWeights[InfluenceIndex] *= InvTotalWeight;
            }
        }
    }
}


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
    OutMesh.SplitStaticMeshes.clear();
    OutMesh.Sockets.clear();
    OutMesh.CollisionShapes.clear();
    OutMesh.NodeMetadata.clear();
    OutMesh.SceneNodes.clear();
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

            if (ImportNode.StaticChildAction == ESkeletalStaticChildImportAction::KeepAsAttachedStaticMesh)
            {
                FString GeneratedStaticMeshPath;
                if (FFbxStaticChildMeshImporter::ImportAttachedStaticMesh(ImportNode.MeshNode, SourcePath, GeneratedStaticMeshPath, BuildContext))
                {
                    StaticChild.StaticMeshAssetPath = GeneratedStaticMeshPath;
                }
            }

            OutMesh.StaticChildMeshes.push_back(StaticChild);
            BuildContext.Summary.StaticChildMeshCount++;

            if (ImportNode.StaticChildAction != ESkeletalStaticChildImportAction::MergeAsRigidPart)
            {
                continue;
            }
        }

        if (ImportNode.Kind == EFbxSkeletalImportMeshKind::Loose)
        {
            FString GeneratedStaticMeshPath;
            if (FFbxStaticChildMeshImporter::ImportLooseStaticMesh(ImportNode.MeshNode, SourcePath, GeneratedStaticMeshPath, BuildContext))
            {
                FFbxSplitStaticMeshReference SplitRef;
                SplitRef.SourceNodeName      = ImportNode.SourceNodeName;
                SplitRef.StaticMeshAssetPath = GeneratedStaticMeshPath;
                SplitRef.GlobalMatrix        = FFbxTransformUtils::ToEngineMatrix(ImportNode.MeshNode->EvaluateGlobalTransform());
                OutMesh.SplitStaticMeshes.push_back(SplitRef);
                BuildContext.Summary.SplitStaticMeshCount++;
            }
            continue;
        }

        if (ImportNode.Kind == EFbxSkeletalImportMeshKind::Ignored || ImportNode.Kind ==
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
        if (!MeshNodesByLOD[LODIndex].empty())
        {
            FFbxSceneQuery::TryGetLODSettings(MeshNodesByLOD[LODIndex][0].MeshNode, NewLOD.ScreenSize, NewLOD.DistanceThreshold);
        }

        OutMesh.LODModels.push_back(NewLOD);
        MorphSourcesByLOD.push_back(std::move(MorphSources));
    }

    if (OutMesh.LODModels.empty())
    {
        return false;
    }

    FFbxMorphTargetImporter::ImportMorphTargets(MorphSourcesByLOD, OutMesh.MorphTargets, BuildContext);

    FFbxAnimationImporter::ImportAnimations(Scene, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, OutMesh.Animations);

    // 여기까지의 skeletal asset-space는 Normalize()가 만든 FBX scene axis 기준이다.
    // Force Front XAxis는 원본 메시 얼굴 방향을 추론하지 않고, scene axis metadata의 front/right/up을
    // 엔진 asset convention(+X Forward / +Y Right / +Z Up)에 맞춰 vertex/bind/animation에 동일하게 bake한다.
    // 단, skeletal path는 ReferenceMeshBindInverse를 통해 FBX SDK ConvertScene()의 global handedness flip을
    // reference mesh 기준공간에서 상쇄할 수 있다. 원본 FBX coord system과 Normalize() 이후 coord system이 다르면
    // side axis mirror를 추가로 bake해서 좌우반전을 제거한다.
    const FbxAxisSystem NormalizedAxisSystem  = Scene->GetGlobalSettings().GetAxisSystem();
    const bool          bMirrorHandedness     = BuildContext.bHasSourceCoordSystem && BuildContext.SourceCoordSystem != NormalizedAxisSystem.GetCoorSystem();
    const FMatrix       EngineAssetCorrection = FFbxTransformUtils::MakeAxisSystemToEngineAssetMatrix(NormalizedAxisSystem, bMirrorHandedness);
    BakeNormalizedSkeletalSpaceToEngineAssetSpace(OutMesh, EngineAssetCorrection);

    OutMesh.Skeleton.SanitizeHierarchyAndBindPose();
    const int32 ValidationBoneCount = static_cast<int32>(OutMesh.Skeleton.Bones.size());
    for (FSkeletalMeshLOD& LOD : OutMesh.LODModels)
    {
        SanitizeLODInfluencesForValidation(LOD, ValidationBoneCount);
    }

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

    FFbxMetadataImporter::CollectSceneNodeMetadata(Scene, OutMesh.NodeMetadata);
    FFbxSceneHierarchyImporter::CollectSceneNodes(Scene, OutMesh.SceneNodes);
    TransformSceneNodesToEngineAssetSpace(OutMesh.SceneNodes, EngineAssetCorrection);
    BuildContext.Summary.MetadataNodeCount = static_cast<int32>(OutMesh.NodeMetadata.size());
    BuildContext.Summary.SceneNodeCount    = static_cast<int32>(OutMesh.SceneNodes.size());

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
