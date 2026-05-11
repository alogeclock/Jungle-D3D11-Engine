#include "Mesh/Fbx/FbxSceneQuery.h"

#include <fbxsdk.h>

#include <algorithm>
#include <cctype>
#include <cstring>

// scene tree에서 mesh attribute를 가진 node를 모두 수집한다.
void FFbxSceneQuery::CollectMeshNodes(FbxNode* Node, TArray<FbxNode*>& OutMeshNodes)
{
    if (!Node)
    {
        return;
    }

    if (FbxNodeAttribute* Attribute = Node->GetNodeAttribute(); Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
    {
        OutMeshNodes.push_back(Node);
    }

    for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
    {
        CollectMeshNodes(Node->GetChild(ChildIndex), OutMeshNodes);
    }
}

// mesh가 skin deformer를 하나 이상 가지고 있는지 확인한다.
bool FFbxSceneQuery::MeshHasSkin(FbxMesh* Mesh)
{
    return Mesh && Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
}

// node attribute가 skeleton인지 확인한다.
bool FFbxSceneQuery::IsSkeletonNode(FbxNode* Node)
{
    if (!Node)
    {
        return false;
    }

    FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
    return Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton;
}

// node 배열에 대상 node 포인터가 이미 들어 있는지 확인한다.
bool FFbxSceneQuery::ContainsNode(const TArray<FbxNode*>& Nodes, const FbxNode* Node)
{
    return std::find(Nodes.begin(), Nodes.end(), Node) != Nodes.end();
}

// node 배열에 중복 없이 node 포인터를 추가한다.
void FFbxSceneQuery::AddUniqueNode(TArray<FbxNode*>& Nodes, FbxNode* Node)
{
    if (Node && !ContainsNode(Nodes, Node))
    {
        Nodes.push_back(Node);
    }
}

// mesh의 skin cluster link bone node를 중복 없이 수집한다.
void FFbxSceneQuery::CollectSkinClusterLinksFromMesh(FbxMesh* Mesh, TArray<FbxNode*>& OutClusterNodes)
{
    if (!Mesh)
    {
        return;
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

            AddUniqueNode(OutClusterNodes, Cluster->GetLink());
        }
    }
}

// node가 scene root인지 확인한다.
bool FFbxSceneQuery::IsSceneRootNode(FbxNode* Node)
{
    return Node && Node->GetParent() == nullptr;
}

// mesh node의 parent chain에서 가장 가까운 skeleton node를 찾는다.
FbxNode* FFbxSceneQuery::FindNearestParentSkeletonNode(FbxNode* MeshNode)
{
    FbxNode* Current = MeshNode ? MeshNode->GetParent() : nullptr;

    while (Current && !IsSceneRootNode(Current))
    {
        if (IsSkeletonNode(Current))
        {
            return Current;
        }

        Current = Current->GetParent();
    }

    return nullptr;
}

// node부터 scene root 직전까지 parent chain node를 중복 없이 추가한다.
void FFbxSceneQuery::AddNodeAndParentsUntilSceneRoot(FbxNode* Node, TArray<FbxNode*>& OutNodes)
{
    FbxNode* Current = Node;

    while (Current && !IsSceneRootNode(Current))
    {
        AddUniqueNode(OutNodes, Current);
        Current = Current->GetParent();
    }
}

// import 대상 bone node 집합에서 root bone 후보를 찾는다.
void FFbxSceneQuery::FindImportedBoneRoot(const TArray<FbxNode*>& Nodes, TArray<FbxNode*>& OutRoots)
{
    for (FbxNode* Node : Nodes)
    {
        if (!Node)
        {
            continue;
        }

        FbxNode* Parent = Node->GetParent();

        if (!Parent || !ContainsNode(Nodes, Parent))
        {
            AddUniqueNode(OutRoots, Node);
        }
    }
}

// 이름의 LOD 접미사에서 LOD index를 파싱한다.
int32 FFbxSceneQuery::ParseLODIndexFromName(const FString& Name)
{
    const char* Patterns[] = { "_LOD", "_lod", "LOD", "lod" };

    for (const char* Pattern : Patterns)
    {
        const size_t Pos = Name.rfind(Pattern);
        if (Pos == FString::npos)
        {
            continue;
        }

        size_t DigitPos = Pos + std::strlen(Pattern);

        if (DigitPos < Name.size() && Name[DigitPos] == '_')
        {
            ++DigitPos;
        }

        if (DigitPos >= Name.size() || !std::isdigit(static_cast<unsigned char>(Name[DigitPos])))
        {
            continue;
        }

        int32 LODIndex = 0;

        while (DigitPos < Name.size() && std::isdigit(static_cast<unsigned char>(Name[DigitPos])))
        {
            LODIndex = LODIndex * 10 + (Name[DigitPos] - '0');
            ++DigitPos;
        }

        return LODIndex;
    }

    return 0;
}

// LODGroup 또는 이름 규칙으로 skeletal mesh LOD index를 계산한다.
int32 FFbxSceneQuery::GetSkeletalMeshLODIndex(FbxNode* MeshNode)
{
    if (!MeshNode)
    {
        return 0;
    }

    FbxNode* Parent = MeshNode->GetParent();
    if (Parent && Parent->GetNodeAttribute() && Parent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
    {
        for (int32 LODIndex = 0; LODIndex < Parent->GetChildCount(); ++LODIndex)
        {
            if (Parent->GetChild(LODIndex) == MeshNode)
            {
                return LODIndex;
            }
        }
    }

    return ParseLODIndexFromName(MeshNode->GetName());
}

// scene 안에 skin deformer를 가진 mesh가 하나라도 있는지 확인한다.
bool FFbxSceneQuery::SceneHasSkinDeformer(FbxScene* Scene)
{
    if (!Scene || !Scene->GetRootNode())
    {
        return false;
    }

    TArray<FbxNode*> MeshNodes;
    CollectMeshNodes(Scene->GetRootNode(), MeshNodes);

    for (FbxNode* MeshNode : MeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
        if (MeshHasSkin(Mesh))
        {
            return true;
        }
    }

    return false;
}
