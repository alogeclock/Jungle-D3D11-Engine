#include "Mesh/Fbx/FbxSceneQuery.h"

#include <fbxsdk.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace
{
    static bool HasSkeletonDescendant(FbxNode* Node)
    {
        if (!Node)
        {
            return false;
        }

        for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
        {
            FbxNode* Child = Node->GetChild(ChildIndex);

            if (FFbxSceneQuery::IsSkeletonNode(Child) || HasSkeletonDescendant(Child))
            {
                return true;
            }
        }

        return false;
    }

    static void CollectFullSkeletonHierarchyRecursive(FbxNode* Node, const TArray<FbxNode*>& SeedNodes, TArray<FbxNode*>& OutBoneNodes)
    {
        if (!Node || FFbxSceneQuery::IsSceneRootNode(Node))
        {
            return;
        }

        const bool bSeedNode      = FFbxSceneQuery::ContainsNode(SeedNodes, Node);
        const bool bSkeletonNode  = FFbxSceneQuery::IsSkeletonNode(Node);
        const bool bKeepContainer = bSeedNode || bSkeletonNode || HasSkeletonDescendant(Node);

        if (!bKeepContainer)
        {
            return;
        }

        FFbxSceneQuery::AddUniqueNode(OutBoneNodes, Node);

        for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
        {
            CollectFullSkeletonHierarchyRecursive(Node->GetChild(ChildIndex), SeedNodes, OutBoneNodes);
        }
    }

    static bool StartsWith(const FString& Value, const char* Prefix)
    {
        return Prefix && Value.rfind(Prefix, 0) == 0;
    }

    static FString ToUpperAscii(FString Value)
    {
        for (char& C : Value)
        {
            C = static_cast<char>(std::toupper(static_cast<unsigned char>(C)));
        }
        return Value;
    }

    static FString StripPrefix(const FString& Value, const char* Prefix)
    {
        if (Prefix && Value.rfind(Prefix, 0) == 0)
        {
            return Value.substr(std::strlen(Prefix));
        }

        return FString();
    }

    static bool IsTransformOnlyNode(FbxNode* Node)
    {
        if (!Node)
        {
            return false;
        }

        FbxNodeAttribute* Attribute = Node->GetNodeAttribute();

        if (!Attribute)
        {
            return true;
        }

        const FbxNodeAttribute::EType Type = Attribute->GetAttributeType();
        return Type == FbxNodeAttribute::eNull || Type == FbxNodeAttribute::eMarker;
    }
}

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

// skeleton root 아래의 full skeleton hierarchy를 수집한다.
void FFbxSceneQuery::CollectFullSkeletonHierarchyFromRoots(const TArray<FbxNode*>& RootNodes, const TArray<FbxNode*>& SeedNodes, TArray<FbxNode*>& OutBoneNodes)
{
    for (FbxNode* RootNode : RootNodes)
    {
        CollectFullSkeletonHierarchyRecursive(RootNode, SeedNodes, OutBoneNodes);
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

FString FFbxSceneQuery::ReadStringProperty(FbxNode* Node, const char* PropertyName)
{
    if (!Node || !PropertyName)
    {
        return FString();
    }

    FbxProperty Property = Node->FindProperty(PropertyName);
    if (!Property.IsValid())
    {
        return FString();
    }

    const FbxDataType DataType = Property.GetPropertyDataType();
    const EFbxType    Type     = DataType.GetType();

    if (Type == eFbxString)
    {
        const FbxString Value = Property.Get<FbxString>();
        return Value.Buffer() ? FString(Value.Buffer()) : FString();
    }

    return FString();
}

bool FFbxSceneQuery::IsCollisionProxyName(const FString& Name)
{
    const FString UpperName = ToUpperAscii(Name);
    return StartsWith(UpperName, "UCX_") || StartsWith(UpperName, "UBX_") || StartsWith(UpperName, "USP_") || StartsWith(UpperName, "UCP_") || StartsWith(
        UpperName,
        "MCDCX_"
    );
}

bool FFbxSceneQuery::IsCollisionProxyNode(FbxNode* Node)
{
    if (!Node)
    {
        return false;
    }

    const FString Name = Node->GetName();
    if (IsCollisionProxyName(Name))
    {
        return true;
    }

    return ReadStringProperty(Node, "ImportKind") == "Collision";
}

int32 FFbxSceneQuery::GetMeshLODIndex(FbxNode* MeshNode)
{
    return GetSkeletalMeshLODIndex(MeshNode);
}

bool FFbxSceneQuery::TryGetFloatProperty(FbxNode* Node, const char* PropertyName, float& OutValue)
{
    if (!Node || !PropertyName)
    {
        return false;
    }

    FbxProperty Property = Node->FindProperty(PropertyName);
    if (!Property.IsValid())
    {
        return false;
    }

    const EFbxType Type = Property.GetPropertyDataType().GetType();
    switch (Type)
    {
    case eFbxFloat:
        OutValue = static_cast<float>(Property.Get<FbxFloat>());
        return true;
    case eFbxDouble:
        OutValue = static_cast<float>(Property.Get<FbxDouble>());
        return true;
    case eFbxInt:
        OutValue = static_cast<float>(Property.Get<FbxInt>());
        return true;
    case eFbxUInt:
        OutValue = static_cast<float>(Property.Get<FbxUInt>());
        return true;
    case eFbxLongLong:
        OutValue = static_cast<float>(Property.Get<FbxLongLong>());
        return true;
    default:
        return false;
    }
}

bool FFbxSceneQuery::TryGetLODSettings(FbxNode* MeshNode, float& OutScreenSize, float& OutDistanceThreshold)
{
    OutScreenSize        = 1.0f;
    OutDistanceThreshold = 0.0f;

    if (!MeshNode)
    {
        return false;
    }

    bool        bFound   = false;
    const int32 LODIndex = GetMeshLODIndex(MeshNode);

    auto TryReadFromNode = [&](FbxNode* Node)
    {
        if (!Node)
        {
            return;
        }

        float Value = 0.0f;
        if (TryGetFloatProperty(Node, "ScreenSize", Value) || TryGetFloatProperty(Node, "LODScreenSize", Value) || TryGetFloatProperty(
            Node,
            "LodScreenSize",
            Value
        ))
        {
            OutScreenSize = Value;
            bFound        = true;
        }

        if (TryGetFloatProperty(Node, "DistanceThreshold", Value) || TryGetFloatProperty(Node, "LODDistance", Value) || TryGetFloatProperty(
            Node,
            "LodDistance",
            Value
        ) || TryGetFloatProperty(Node, "LODThreshold", Value))
        {
            OutDistanceThreshold = Value;
            bFound               = true;
        }
    };

    TryReadFromNode(MeshNode);

    FbxNode* Parent = MeshNode->GetParent();
    if (Parent && Parent->GetNodeAttribute() && Parent->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
    {
        TryReadFromNode(Parent);

        const FString IndexSuffix = std::to_string(LODIndex);
        float         Value       = 0.0f;
        if (TryGetFloatProperty(Parent, ("LOD" + IndexSuffix + "_ScreenSize").c_str(), Value) || TryGetFloatProperty(
            Parent,
            ("LOD" + IndexSuffix + "ScreenSize").c_str(),
            Value
        ))
        {
            OutScreenSize = Value;
            bFound        = true;
        }
        if (TryGetFloatProperty(Parent, ("LOD" + IndexSuffix + "_Distance").c_str(), Value) || TryGetFloatProperty(
            Parent,
            ("LOD" + IndexSuffix + "Distance").c_str(),
            Value
        ) || TryGetFloatProperty(Parent, ("LOD" + IndexSuffix + "_Threshold").c_str(), Value))
        {
            OutDistanceThreshold = Value;
            bFound               = true;
        }
    }

    return bFound;
}

bool FFbxSceneQuery::IsSocketName(const FString& Name)
{
    return StartsWith(Name, "SOCKET_") || StartsWith(Name, "Socket_") || StartsWith(Name, "socket_");
}

bool FFbxSceneQuery::IsSocketNode(FbxNode* Node)
{
    if (!Node)
    {
        return false;
    }

    if (IsSkeletonNode(Node))
    {
        return false;
    }

    if (FbxNodeAttribute* Attribute = Node->GetNodeAttribute())
    {
        if (Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            return false;
        }
    }

    if (!IsTransformOnlyNode(Node))
    {
        return false;
    }

    const FString ImportKind = ReadStringProperty(Node, "ImportKind");

    if (ImportKind == "Socket")
    {
        return true;
    }

    return IsSocketName(Node->GetName());
}

FString FFbxSceneQuery::GetSocketName(FbxNode* Node)
{
    if (!Node)
    {
        return FString();
    }

    const FString ExplicitName = ReadStringProperty(Node, "SocketName");

    if (!ExplicitName.empty())
    {
        return ExplicitName;
    }

    const FString NodeName = Node->GetName();

    FString Stripped = StripPrefix(NodeName, "SOCKET_");
    if (!Stripped.empty())
    {
        return Stripped;
    }

    Stripped = StripPrefix(NodeName, "Socket_");
    if (!Stripped.empty())
    {
        return Stripped;
    }

    Stripped = StripPrefix(NodeName, "socket_");
    if (!Stripped.empty())
    {
        return Stripped;
    }

    return NodeName;
}

void FFbxSceneQuery::CollectSocketNodes(FbxNode* Node, TArray<FbxNode*>& OutSocketNodes)
{
    if (!Node)
    {
        return;
    }

    if (IsSocketNode(Node))
    {
        OutSocketNodes.push_back(Node);
    }

    for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
    {
        CollectSocketNodes(Node->GetChild(ChildIndex), OutSocketNodes);
    }
}

bool FFbxSceneQuery::FindNearestParentBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& BoneNodeToIndex, FbxNode*& OutBoneNode, int32& OutBoneIndex)
{
    FbxNode* Current = Node ? Node->GetParent() : nullptr;

    while (Current && !IsSceneRootNode(Current))
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
