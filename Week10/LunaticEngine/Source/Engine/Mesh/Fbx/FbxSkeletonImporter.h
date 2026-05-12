#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>
#include "Math/Matrix.h"
#include "Mesh/SkeletalMeshAsset.h"

struct FFbxImportContext;

class FFbxSkeletonImporter
{
public:
    // skin cluster link node와 parent chain을 기반으로 skeleton hierarchy를 구성한다.
    static bool BuildSkeletonFromSkinClusters(
        const TArray<FbxNode*>& SkinnedMeshNodes,
        const TArray<FbxNode*>& AllMeshNodes,
        FSkeleton&              OutSkeleton,
        TMap<FbxNode*, int32>&  OutBoneNodeToIndex
        );

    // FBX bind pose 또는 cluster transform matrix에서 첫 mesh bind matrix를 찾는다.
    static bool TryGetFirstMeshBindMatrix(FbxScene* Scene, FbxNode* MeshNode, FMatrix& OutMeshBindMatrix);

    // reference LOD mesh들의 기준 bind matrix를 찾는다.
    static bool TryGetReferenceMeshBindMatrix(FbxScene* Scene, const TArray<FbxNode*>& SkinnedMeshNodes, FMatrix& OutReferenceMeshBindMatrix);

    // scene global transform으로 skeleton global bind pose를 초기화한다.
    static void InitializeBoneBindPoseFromSceneNodes(
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        const FMatrix&               ReferenceMeshBindInverse,
        FSkeleton&                   Skeleton
        );

    // FBX pose 정보로 skeleton bind pose를 보정한다.
    static void ApplyBindPoseFromFbxPose(
        FbxScene*                    Scene,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        const FMatrix&               ReferenceMeshBindInverse,
        FSkeleton&                   Skeleton,
        TArray<bool>&                AppliedClusterBindPose,
        FFbxImportContext&           BuildContext
        );

    // skin cluster transform link matrix로 skeleton bind pose를 보정한다.
    static void ApplyBindPoseFromSkinClusters(
        const TArray<FbxNode*>&      SkinnedMeshNodes,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        const FMatrix&               ReferenceMeshBindInverse,
        FSkeleton&                   Skeleton,
        TArray<bool>*                AppliedClusterBindPose,
        bool                         bOverrideExisting
        );

    // cluster bind matrix가 없는 bone을 parent bind pose 기준으로 최종 정렬한다.
    static void FinalizeNonClusterBoneBindPose(
        FbxScene*                    Scene,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        const FMatrix&               ReferenceMeshBindInverse,
        const TArray<bool>&          ClusterAppliedBoneMask,
        FSkeleton&                   Skeleton,
        FFbxImportContext&           BuildContext
        );

    // global bind pose를 기준으로 local bind pose와 inverse bind pose를 재계산한다.
    static void RecomputeLocalBindPose(FSkeleton& Skeleton);
};
