#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Mesh/SkeletalMeshAsset.h"

#include <fbxsdk.h>

enum class EFbxSkeletalImportMeshKind : uint8
{
    Skinned,

    // Skin deformer는 없지만 skeleton bone 아래에 있는 static mesh.
    // 이것은 구조적 사실만 의미한다. 병합/보존/무시는 StaticChildAction으로 결정한다.
    StaticChildOfBone,

    // UCX_/UBX_/USP_/UCP_ 같은 명시적 collision proxy.
    CollisionProxy,

    // skeleton과 무관한 mesh.
    Loose,

    // FBX custom property ImportKind=Ignore로 명시된 node.
    Ignored
};

struct FFbxSkeletalImportMeshNode
{
    FbxNode* MeshNode = nullptr;

    EFbxSkeletalImportMeshKind Kind = EFbxSkeletalImportMeshKind::Loose;

    int32    ParentBoneIndex = -1;
    FbxNode* ParentBoneNode  = nullptr;

    FString SourceNodeName;

    // Mesh node의 geometric/global transform을 parent bone global transform 기준 local matrix로 저장한다.
    FMatrix LocalMatrixToParentBone = FMatrix::Identity;

    // Kind == StaticChildOfBone일 때만 의미가 있다.
    ESkeletalStaticChildImportAction StaticChildAction = ESkeletalStaticChildImportAction::MergeAsRigidPart;
};

class FFbxSkeletalMeshClassifier
{
public:
    // mesh node들을 skinned, static child, collision proxy, loose로 구조 분류한다.
    static void Classify(const TArray<FbxNode*>& MeshNodes, const TMap<FbxNode*, int32>& BoneNodeToIndex, TArray<FFbxSkeletalImportMeshNode>& OutImportNodes);
};
