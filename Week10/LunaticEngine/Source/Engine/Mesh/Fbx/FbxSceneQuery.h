#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>

class FFbxSceneQuery
{
public:
    // scene tree에서 mesh attribute를 가진 node를 모두 수집한다.
    static void CollectMeshNodes(FbxNode* Node, TArray<FbxNode*>& OutMeshNodes);

    // mesh가 skin deformer를 하나 이상 가지고 있는지 확인한다.
    static bool MeshHasSkin(FbxMesh* Mesh);

    // node attribute가 skeleton인지 확인한다.
    static bool IsSkeletonNode(FbxNode* Node);

    // node 배열에 대상 node 포인터가 이미 들어 있는지 확인한다.
    static bool ContainsNode(const TArray<FbxNode*>& Nodes, const FbxNode* Node);
    
    // node 배열에 중복 없이 node 포인터를 추가한다.
    static void AddUniqueNode(TArray<FbxNode*>& Nodes, FbxNode* Node);

    // mesh의 skin cluster link bone node를 중복 없이 수집한다.
    static void CollectSkinClusterLinksFromMesh(FbxMesh* Mesh, TArray<FbxNode*>& OutClusterNodes);

    // node가 scene root인지 확인한다.
    static bool IsSceneRootNode(FbxNode* Node);

    // mesh node의 parent chain에서 가장 가까운 skeleton node를 찾는다.
    static FbxNode* FindNearestParentSkeletonNode(FbxNode* MeshNode);

    // node부터 scene root 직전까지 parent chain node를 중복 없이 추가한다.
    static void AddNodeAndParentsUntilSceneRoot(FbxNode* Node, TArray<FbxNode*>& OutNodes);

    // import 대상 bone node 집합에서 root bone 후보를 찾는다.
    static void FindImportedBoneRoot(const TArray<FbxNode*>& Nodes, TArray<FbxNode*>& OutRoots);

    // skeleton root 아래의 full skeleton hierarchy를 수집한다.
    static void CollectFullSkeletonHierarchyFromRoots(const TArray<FbxNode*>& RootNodes, const TArray<FbxNode*>& SeedNodes, TArray<FbxNode*>& OutBoneNodes);

    // 이름의 LOD 접미사에서 LOD index를 파싱한다.
    static int32 ParseLODIndexFromName(const FString& Name);

    // LODGroup 또는 이름 규칙으로 skeletal mesh LOD index를 계산한다.
    static int32 GetSkeletalMeshLODIndex(FbxNode* MeshNode);

    // scene 안에 skin deformer를 가진 mesh가 하나라도 있는지 확인한다.
    static bool SceneHasSkinDeformer(FbxScene* Scene);

    static FString ReadStringProperty(FbxNode* Node, const char* PropertyName);

    static bool IsCollisionProxyName(const FString& Name);

    static bool IsCollisionProxyNode(FbxNode* Node);

    static int32 GetMeshLODIndex(FbxNode* MeshNode);

    static bool IsSocketName(const FString& Name);

    static bool IsSocketNode(FbxNode* Node);

    static FString GetSocketName(FbxNode* Node);

    static void CollectSocketNodes(FbxNode* Node, TArray<FbxNode*>& OutSocketNodes);

    static bool FindNearestParentBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& BoneNodeToIndex, FbxNode*& OutBoneNode, int32& OutBoneIndex);
};
