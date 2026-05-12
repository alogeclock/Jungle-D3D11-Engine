#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>

enum class EFbxSkeletalImportMeshKind : uint8
{
    Skinned,
    RigidAttached,
    Loose
};

struct FFbxSkeletalImportMeshNode
{
    FbxNode*                   MeshNode       = nullptr;
    EFbxSkeletalImportMeshKind Kind           = EFbxSkeletalImportMeshKind::Loose;
    int32                      RigidBoneIndex = -1;
    FbxNode*                   RigidBoneNode  = nullptr;
};

class FFbxSkeletalMeshClassifier
{
public:
    // mesh node들을 skinned, rigid attached, loose로 분류한다.
    static void Classify(const TArray<FbxNode*>& MeshNodes, const TMap<FbxNode*, int32>& BoneNodeToIndex, TArray<FFbxSkeletalImportMeshNode>& OutImportNodes);
};
