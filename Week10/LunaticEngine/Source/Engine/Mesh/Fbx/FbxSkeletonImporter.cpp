#include "Mesh/Fbx/FbxSkeletonImporter.h"

#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxTransformUtils.h"

#include <fbxsdk.h>

namespace
{
    // FBX bone node를 Skeleton에 추가하고 import 대상 child bone을 재귀 등록한다.
    static int32 AddImportedBoneRecursive(
        FbxNode*                BoneNode,
        int32                   ParentIndex,
        const TArray<FbxNode*>& ImportedBoneNodes,
        FSkeleton&              OutSkeleton,
        TMap<FbxNode*, int32>&  OutBoneNodeToIndex
        )
    {
        if (!BoneNode || !FFbxSceneQuery::ContainsNode(ImportedBoneNodes, BoneNode))
        {
            return -1;
        }

        auto Existing = OutBoneNodeToIndex.find(BoneNode);
        if (Existing != OutBoneNodeToIndex.end()) return Existing->second;

        const int32 BoneIndex = static_cast<int32>(OutSkeleton.Bones.size());

        FBoneInfo Bone;
        Bone.Name        = BoneNode->GetName();
        Bone.ParentIndex = ParentIndex;

        const FbxAMatrix LocalBind  = BoneNode->EvaluateLocalTransform();
        const FbxAMatrix GlobalBind = BoneNode->EvaluateGlobalTransform();

        Bone.LocalBindPose   = FFbxTransformUtils::ToEngineMatrix(LocalBind);
        Bone.GlobalBindPose  = FFbxTransformUtils::ToEngineMatrix(GlobalBind);
        Bone.InverseBindPose = Bone.GlobalBindPose.GetInverse();

        OutSkeleton.Bones.push_back(Bone);
        OutBoneNodeToIndex[BoneNode] = BoneIndex;

        for (int32 ChildIndex = 0; ChildIndex < BoneNode->GetChildCount(); ++ChildIndex)
        {
            FbxNode* Child = BoneNode->GetChild(ChildIndex);

            if (FFbxSceneQuery::ContainsNode(ImportedBoneNodes, Child))
            {
                AddImportedBoneRecursive(Child, BoneIndex, ImportedBoneNodes, OutSkeleton, OutBoneNodeToIndex);
            }
        }
        return BoneIndex;
    }

    // FBX bind pose에서 node의 bind matrix를 찾는다.
    static bool TryGetBindPoseMatrixForNode(FbxScene* Scene, FbxNode* Node, FMatrix& OutPoseMatrix)
    {
        if (!Scene || !Node)
        {
            return false;
        }

        const int32 PoseCount = Scene->GetPoseCount();

        for (int32 PoseIndex = 0; PoseIndex < PoseCount; ++PoseIndex)
        {
            FbxPose* Pose = Scene->GetPose(PoseIndex);
            if (!Pose || !Pose->IsBindPose())
            {
                continue;
            }

            const int32 NodeIndex = Pose->Find(Node);
            if (NodeIndex < 0)
            {
                continue;
            }

            OutPoseMatrix = FFbxTransformUtils::ToEngineMatrix(Pose->GetMatrix(NodeIndex));
            return true;
        }

        return false;
    }

    // rigid attached mesh의 bind matrix를 pose 또는 global transform으로 계산한다.
    static FMatrix GetRigidMeshBindMatrix(FbxScene* Scene, FbxNode* MeshNode)
    {
        if (!MeshNode)
        {
            return FMatrix::Identity;
        }

        const FMatrix GeometryTransform = FFbxTransformUtils::ToEngineMatrix(FFbxTransformUtils::GetNodeGeometryTransform(MeshNode));

        FMatrix PoseMatrix;
        if (TryGetBindPoseMatrixForNode(Scene, MeshNode, PoseMatrix))
        {
            return GeometryTransform * PoseMatrix;
        }

        return GeometryTransform * FFbxTransformUtils::ToEngineMatrix(MeshNode->EvaluateGlobalTransform());
    }

    static bool IsValidBoneIndex(const FSkeleton& Skeleton, int32 BoneIndex)
    {
        return BoneIndex >= 0 && BoneIndex < static_cast<int32>(Skeleton.Bones.size());
    }

    static TArray<FbxNode*> BuildBoneNodesByIndex(
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        const FSkeleton&             Skeleton
        )
    {
        TArray<FbxNode*> BoneNodesByIndex;
        BoneNodesByIndex.resize(Skeleton.Bones.size(), nullptr);

        for (const auto& Pair : BoneNodeToIndex)
        {
            FbxNode*    BoneNode  = Pair.first;
            const int32 BoneIndex = Pair.second;

            if (BoneNode && IsValidBoneIndex(Skeleton, BoneIndex))
            {
                BoneNodesByIndex[BoneIndex] = BoneNode;
            }
        }

        return BoneNodesByIndex;
    }

    static FMatrix GetSceneGlobalMatrix(FbxNode* Node)
    {
        if (!Node)
        {
            return FMatrix::Identity;
        }

        return FFbxTransformUtils::ToEngineMatrix(Node->EvaluateGlobalTransform());
    }

    static bool TryGetBindPoseGlobalMatrix(FbxScene* Scene, FbxNode* Node, FMatrix& OutGlobalMatrix)
    {
        return TryGetBindPoseMatrixForNode(Scene, Node, OutGlobalMatrix);
    }

    static FMatrix GetSceneLocalMatrixFromGlobals(FbxNode* ChildNode, FbxNode* ParentNode)
    {
        if (!ChildNode)
        {
            return FMatrix::Identity;
        }

        const FMatrix ChildGlobal = GetSceneGlobalMatrix(ChildNode);

        if (!ParentNode || FFbxSceneQuery::IsSceneRootNode(ParentNode))
        {
            return ChildGlobal;
        }

        const FMatrix ParentGlobal = GetSceneGlobalMatrix(ParentNode);
        return ChildGlobal * ParentGlobal.GetInverse();
    }

    static bool TryGetSourceLocalBindMatrix(
        FbxScene* Scene,
        FbxNode* ChildNode,
        FbxNode* ParentNode,
        FMatrix& OutLocalMatrix
        )
    {
        if (!ChildNode)
        {
            return false;
        }

        FMatrix ChildPoseGlobal;
        if (!TryGetBindPoseGlobalMatrix(Scene, ChildNode, ChildPoseGlobal))
        {
            return false;
        }

        if (!ParentNode || FFbxSceneQuery::IsSceneRootNode(ParentNode))
        {
            OutLocalMatrix = ChildPoseGlobal;
            return true;
        }

        FMatrix ParentPoseGlobal;
        if (!TryGetBindPoseGlobalMatrix(Scene, ParentNode, ParentPoseGlobal))
        {
            return false;
        }

        OutLocalMatrix = ChildPoseGlobal * ParentPoseGlobal.GetInverse();
        return true;
    }

    static FMatrix GetBestSourceLocalBindMatrix(
        FbxScene* Scene,
        FbxNode* ChildNode,
        FbxNode* ParentNode,
        bool&     bUsedSceneFallback
        )
    {
        FMatrix LocalMatrix;
        if (TryGetSourceLocalBindMatrix(Scene, ChildNode, ParentNode, LocalMatrix))
        {
            return LocalMatrix;
        }

        bUsedSceneFallback = true;
        return GetSceneLocalMatrixFromGlobals(ChildNode, ParentNode);
    }
}

// skin cluster link node와 parent chain을 기반으로 skeleton hierarchy를 구성한다.
bool FFbxSkeletonImporter::BuildSkeletonFromSkinClusters(
    const TArray<FbxNode*>& SkinnedMeshNodes,
    const TArray<FbxNode*>& AllMeshNodes,
    FSkeleton&              OutSkeleton,
    TMap<FbxNode*, int32>&  OutBoneNodeToIndex
    )
{
    OutSkeleton.Bones.clear();
    OutBoneNodeToIndex.clear();

    TArray<FbxNode*> LinkNodes;

    for (FbxNode* MeshNode : SkinnedMeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (Mesh && FFbxSceneQuery::MeshHasSkin(Mesh))
        {
            FFbxSceneQuery::CollectSkinClusterLinksFromMesh(Mesh, LinkNodes);
        }
    }

    if (LinkNodes.empty())
    {
        return false;
    }

    TArray<FbxNode*> ImportedBoneNodes;

    for (FbxNode* LinkNode : LinkNodes)
    {
        FFbxSceneQuery::AddNodeAndParentsUntilSceneRoot(LinkNode, ImportedBoneNodes);
    }

    // skin은 없지만 skeleton bone parent 아래에 붙은 mesh를 rigid attachment로 처리할 수 있도록,
    // 해당 parent skeleton bone도 skeleton 후보에 포함한다.
    for (FbxNode* MeshNode : AllMeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (!Mesh || FFbxSceneQuery::MeshHasSkin(Mesh))
        {
            continue;
        }

        FbxNode* RigidParentBone = FFbxSceneQuery::FindNearestParentSkeletonNode(MeshNode);
        if (RigidParentBone)
        {
            FFbxSceneQuery::AddNodeAndParentsUntilSceneRoot(RigidParentBone, ImportedBoneNodes);
        }
    }

    TArray<FbxNode*> RootBones;

    FFbxSceneQuery::FindImportedBoneRoot(ImportedBoneNodes, RootBones);

    // cluster link와 parent chain을 seed로 삼되, skeleton root 아래의 full hierarchy를 포함한다.
    TArray<FbxNode*> FullBoneNodes;
    FFbxSceneQuery::CollectFullSkeletonHierarchyFromRoots(RootBones, ImportedBoneNodes, FullBoneNodes);

    if (FullBoneNodes.empty())
    {
        FullBoneNodes = ImportedBoneNodes;
    }

    TArray<FbxNode*> FullRootBones;
    FFbxSceneQuery::FindImportedBoneRoot(FullBoneNodes, FullRootBones);

    for (FbxNode* RootBone : FullRootBones)
    {
        AddImportedBoneRecursive(
            RootBone,
            -1,
            FullBoneNodes,
            OutSkeleton,
            OutBoneNodeToIndex
        );
    }
    
    OutSkeleton.RebuildChildren();

    return !OutSkeleton.Bones.empty();
}

// FBX bind pose 또는 cluster transform matrix에서 첫 mesh bind matrix를 찾는다.
bool FFbxSkeletonImporter::TryGetFirstMeshBindMatrix(FbxScene* Scene, FbxNode* MeshNode, FMatrix& OutMeshBindMatrix)
{
    FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
    if (!Mesh)
    {
        return false;
    }

    const FMatrix GeometryTransform = FFbxTransformUtils::ToEngineMatrix(FFbxTransformUtils::GetNodeGeometryTransform(MeshNode));

    FMatrix PoseMatrix;
    if (TryGetBindPoseMatrixForNode(Scene, MeshNode, PoseMatrix))
    {
        OutMeshBindMatrix = GeometryTransform * PoseMatrix;
        return true;
    }

    const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

    for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
    {
        FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
        if (!Skin)
        {
            continue;
        }

        const int32 ClusterCount = Skin->GetClusterCount();

        for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
        {
            FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
            if (!Cluster)
            {
                continue;
            }

            FbxAMatrix MeshNodeBindFbx;
            Cluster->GetTransformMatrix(MeshNodeBindFbx);

            const FMatrix MeshNodeBind = FFbxTransformUtils::ToEngineMatrix(MeshNodeBindFbx);

            OutMeshBindMatrix = GeometryTransform * MeshNodeBind;
            return true;
        }
    }

    return false;
}

// reference LOD mesh들의 기준 bind matrix를 찾는다.
bool FFbxSkeletonImporter::TryGetReferenceMeshBindMatrix(FbxScene* Scene, const TArray<FbxNode*>& SkinnedMeshNodes, FMatrix& OutReferenceMeshBindMatrix)
{
    for (FbxNode* MeshNode : SkinnedMeshNodes)
    {
        if (FFbxSkeletonImporter::TryGetFirstMeshBindMatrix(Scene, MeshNode, OutReferenceMeshBindMatrix))
        {
            return true;
        }
    }

    return false;
}

// scene global transform으로 skeleton global bind pose를 초기화한다.
void FFbxSkeletonImporter::InitializeBoneBindPoseFromSceneNodes(
    const TMap<FbxNode*, int32>& BoneNodeToIndex,
    const FMatrix&               ReferenceMeshBindInverse,
    FSkeleton&                   Skeleton
    )
{
    for (const auto& Pair : BoneNodeToIndex)
    {
        FbxNode*    BoneNode  = Pair.first;
        const int32 BoneIndex = Pair.second;

        if (!BoneNode || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
        {
            continue;
        }

        const FMatrix BoneGlobal = FFbxTransformUtils::ToEngineMatrix(BoneNode->EvaluateGlobalTransform());

        const FMatrix BoneInReferenceMeshSpace = BoneGlobal * ReferenceMeshBindInverse;

        Skeleton.Bones[BoneIndex].GlobalBindPose  = BoneInReferenceMeshSpace;
        Skeleton.Bones[BoneIndex].InverseBindPose = BoneInReferenceMeshSpace.GetInverse();
    }
}

// FBX pose 정보로 skeleton bind pose를 보정한다.
void FFbxSkeletonImporter::ApplyBindPoseFromFbxPose(
    FbxScene*                    Scene,
    const TMap<FbxNode*, int32>& BoneNodeToIndex,
    const FMatrix&               ReferenceMeshBindInverse,
    FSkeleton&                   Skeleton,
    TArray<bool>&                InOutAppliedBoneMask,
    FFbxImportContext&           BuildContext
    )
{
    if (!Scene)
    {
        return;
    }

    bool bFoundAnyBindPose = false;

    for (const auto& Pair : BoneNodeToIndex)
    {
        FbxNode*    BoneNode  = Pair.first;
        const int32 BoneIndex = Pair.second;

        if (!BoneNode || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
        {
            continue;
        }

        FMatrix BonePoseMatrix;
        if (!TryGetBindPoseMatrixForNode(Scene, BoneNode, BonePoseMatrix))
        {
            continue;
        }

        bFoundAnyBindPose = true;

        const FMatrix BoneInReferenceMeshSpace = BonePoseMatrix * ReferenceMeshBindInverse;

        Skeleton.Bones[BoneIndex].GlobalBindPose  = BoneInReferenceMeshSpace;
        Skeleton.Bones[BoneIndex].InverseBindPose = BoneInReferenceMeshSpace.GetInverse();

        if (BoneIndex < static_cast<int32>(InOutAppliedBoneMask.size()))
        {
            InOutAppliedBoneMask[BoneIndex] = true;
        }
    }

    if (!bFoundAnyBindPose)
    {
        BuildContext.AddWarning(
            ESkeletalImportWarningType::MissingBindPose,
            "No explicit FBX bind pose was found. Falling back to skin cluster bind matrices."
        );
    }
}

// skin cluster transform link matrix로 skeleton bind pose를 보정한다.
void FFbxSkeletonImporter::ApplyBindPoseFromSkinClusters(
    const TArray<FbxNode*>&      SkinnedMeshNodes,
    const TMap<FbxNode*, int32>& BoneNodeToIndex,
    const FMatrix&               ReferenceMeshBindInverse,
    FSkeleton&                   Skeleton,
    TArray<bool>*                InOutAppliedBoneMask,
    bool                         bOverrideExisting
    )
{
    for (FbxNode* MeshNode : SkinnedMeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
        if (!Mesh)
        {
            continue;
        }

        const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

        for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
        {
            FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
            if (!Skin)
            {
                continue;
            }

            const int32 ClusterCount = Skin->GetClusterCount();

            for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
            {
                FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
                if (!Cluster || !Cluster->GetLink())
                {
                    continue;
                }

                auto BoneIt = BoneNodeToIndex.find(Cluster->GetLink());
                if (BoneIt == BoneNodeToIndex.end())
                {
                    continue;
                }

                const int32 BoneIndex = BoneIt->second;

                if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
                {
                    continue;
                }

                if (!bOverrideExisting &&
                    InOutAppliedBoneMask &&
                    BoneIndex < static_cast<int32>(InOutAppliedBoneMask->size()) &&
                    (*InOutAppliedBoneMask)[BoneIndex])
                {
                    continue;
                }

                FbxAMatrix LinkBindFbx;
                Cluster->GetTransformLinkMatrix(LinkBindFbx);

                const FMatrix LinkBind = FFbxTransformUtils::ToEngineMatrix(LinkBindFbx);

                const FMatrix BoneBindInReferenceMeshSpace = LinkBind * ReferenceMeshBindInverse;

                Skeleton.Bones[BoneIndex].GlobalBindPose  = BoneBindInReferenceMeshSpace;
                Skeleton.Bones[BoneIndex].InverseBindPose = BoneBindInReferenceMeshSpace.GetInverse();

                if (InOutAppliedBoneMask && BoneIndex < static_cast<int32>(InOutAppliedBoneMask->size()))
                {
                    (*InOutAppliedBoneMask)[BoneIndex] = true;
                }
            }
        }
    }
}

// cluster bind matrix가 없는 bone을 parent bind pose 기준으로 최종 정렬한다.
void FFbxSkeletonImporter::FinalizeNonClusterBoneBindPose(
    FbxScene*                    Scene,
    const TMap<FbxNode*, int32>& BoneNodeToIndex,
    const FMatrix&               ReferenceMeshBindInverse,
    const TArray<bool>&          ClusterAppliedBoneMask,
    FSkeleton&                   Skeleton,
    FFbxImportContext&           BuildContext
    )
{
    const TArray<FbxNode*> BoneNodesByIndex = BuildBoneNodesByIndex(BoneNodeToIndex, Skeleton);

    int32 FinalizedBoneCount = 0;
    int32 SceneFallbackBoneCount = 0;

    // AddImportedBoneRecursive는 parent를 child보다 먼저 넣는다.
    // 정방향으로 돌면 parent의 최종 GlobalBindPose를 기준으로 child를 안정적으로 재구성할 수 있다.
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
    {
        const bool bClusterApplied =
            BoneIndex < static_cast<int32>(ClusterAppliedBoneMask.size()) &&
            ClusterAppliedBoneMask[BoneIndex];

        if (bClusterApplied)
        {
            // weighted bone은 skin cluster bind matrix가 최종 source of truth다.
            Skeleton.Bones[BoneIndex].InverseBindPose =
                Skeleton.Bones[BoneIndex].GlobalBindPose.GetInverse();
            continue;
        }

        FbxNode* BoneNode = BoneNodesByIndex[BoneIndex];
        if (!BoneNode)
        {
            continue;
        }

        FBoneInfo& Bone = Skeleton.Bones[BoneIndex];
        bool bUsedSceneFallback = false;

        if (IsValidBoneIndex(Skeleton, Bone.ParentIndex))
        {
            FbxNode* ParentNode = BoneNodesByIndex[Bone.ParentIndex];

            Bone.LocalBindPose = GetBestSourceLocalBindMatrix(
                Scene,
                BoneNode,
                ParentNode,
                bUsedSceneFallback
            );

            Bone.GlobalBindPose =
                Bone.LocalBindPose *
                Skeleton.Bones[Bone.ParentIndex].GlobalBindPose;
        }
        else
        {
            FMatrix RootPoseGlobal;
            if (TryGetBindPoseGlobalMatrix(Scene, BoneNode, RootPoseGlobal))
            {
                Bone.GlobalBindPose = RootPoseGlobal * ReferenceMeshBindInverse;
            }
            else
            {
                bUsedSceneFallback = true;
                Bone.GlobalBindPose = GetSceneGlobalMatrix(BoneNode) * ReferenceMeshBindInverse;
            }

            Bone.LocalBindPose = Bone.GlobalBindPose;
        }

        Bone.InverseBindPose = Bone.GlobalBindPose.GetInverse();
        ++FinalizedBoneCount;

        if (bUsedSceneFallback)
        {
            ++SceneFallbackBoneCount;
        }
    }

    if (FinalizedBoneCount > 0)
    {
        BuildContext.AddWarningOnce(
            ESkeletalImportWarningType::UsedClusterBindPoseFallback,
            "One or more non-weighted bones were aligned to their parent bind pose hierarchy."
        );
    }

    if (SceneFallbackBoneCount > 0)
    {
        BuildContext.AddWarningOnce(
            ESkeletalImportWarningType::UsedSceneTransformFallback,
            "One or more bones had no FBX bind pose. Scene transforms were used only as local hierarchy fallback."
        );
    }
}

// global bind pose를 기준으로 local bind pose와 inverse bind pose를 재계산한다.
void FFbxSkeletonImporter::RecomputeLocalBindPose(FSkeleton& Skeleton)
{
    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
    {
        FBoneInfo& Bone = Skeleton.Bones[BoneIndex];

        if (Bone.ParentIndex >= 0 && Bone.ParentIndex < static_cast<int32>(Skeleton.Bones.size()))
        {
            const FMatrix ParentGlobal = Skeleton.Bones[Bone.ParentIndex].GlobalBindPose;
            Bone.LocalBindPose         = Bone.GlobalBindPose * ParentGlobal.GetInverse();
        }
        else
        {
            Bone.LocalBindPose = Bone.GlobalBindPose;
        }

        Bone.InverseBindPose = Bone.GlobalBindPose.GetInverse();
    }

    Skeleton.RebuildChildren();
}
