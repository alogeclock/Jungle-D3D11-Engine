#include "Mesh/Fbx/FbxSkeletalMeshClassifier.h"

#include "Mesh/Fbx/FbxSceneQuery.h"

#include <fbxsdk.h>

namespace
{
    // mesh node parent chain에서 skeleton bone index를 찾는다.
    static bool FindNearestParentBoneIndex(FbxNode* MeshNode, const TMap<FbxNode*, int32>& BoneNodeToIndex, FbxNode*& OutBoneNode, int32& OutBoneIndex)
    {
        FbxNode* Current = MeshNode ? MeshNode->GetParent() : nullptr;

        while (Current && !FFbxSceneQuery::IsSceneRootNode(Current))
        {
            auto BoneIt = BoneNodeToIndex.find(Current);
            if (BoneIt != BoneNodeToIndex.end())
            {
                OutBoneNode  = Current;
                OutBoneIndex = BoneIt->second;
                return true;
            }

            Current = Current->GetParent();
        }

        OutBoneNode  = nullptr;
        OutBoneIndex = -1;
        return false;
    }
}

void FFbxSkeletalMeshClassifier::Classify(
    const TArray<FbxNode*>&             MeshNodes,
    const TMap<FbxNode*, int32>&        BoneNodeToIndex,
    TArray<FFbxSkeletalImportMeshNode>& OutImportNodes
    )
{
    OutImportNodes.clear();

    for (FbxNode* MeshNode : MeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
        if (!Mesh)
        {
            continue;
        }

        FFbxSkeletalImportMeshNode ImportNode;
        ImportNode.MeshNode = MeshNode;

        if (FFbxSceneQuery::MeshHasSkin(Mesh))
        {
            ImportNode.Kind = EFbxSkeletalImportMeshKind::Skinned;
            OutImportNodes.push_back(ImportNode);
            continue;
        }

        int32    ParentBoneIndex = -1;
        FbxNode* ParentBoneNode  = nullptr;
        if (FindNearestParentBoneIndex(MeshNode, BoneNodeToIndex, ParentBoneNode, ParentBoneIndex))
        {
            ImportNode.Kind           = EFbxSkeletalImportMeshKind::RigidAttached;
            ImportNode.RigidBoneIndex = ParentBoneIndex;
            ImportNode.RigidBoneNode  = ParentBoneNode;
            OutImportNodes.push_back(ImportNode);
            continue;
        }

        ImportNode.Kind = EFbxSkeletalImportMeshKind::Loose;
        OutImportNodes.push_back(ImportNode);
    }
}
