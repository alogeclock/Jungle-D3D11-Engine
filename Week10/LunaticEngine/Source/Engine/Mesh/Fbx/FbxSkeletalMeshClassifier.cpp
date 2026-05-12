#include "Mesh/Fbx/FbxSkeletalMeshClassifier.h"

#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxTransformUtils.h"

#include <fbxsdk.h>

namespace
{
    static bool StartsWith(const FString& Value, const char* Prefix)
    {
        return Prefix && Value.rfind(Prefix, 0) == 0;
    }

    static bool IsCollisionProxyName(const FString& Name)
    {
        // 명시적 예약 prefix
        return StartsWith(Name, "UCX_") || StartsWith(Name, "UBX_") || StartsWith(Name, "USP_") || StartsWith(Name, "UCP_") || StartsWith(Name, "MCDCX_");
    }

    static FString ReadStringProperty(FbxNode* Node, const char* PropertyName)
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

    static ESkeletalStaticChildImportAction ReadStaticChildAction(FbxNode* Node)
    {
        // 지원 값:
        //   ImportKind=RigidAttached / MergeAsRigidPart / StaticChild
        //   ImportKind=Attachment / KeepAsAttachedStaticMesh
        //   ImportKind=Ignore
        const FString ImportKind = ReadStringProperty(Node, "ImportKind");

        if (ImportKind == "Attachment" || ImportKind == "KeepAsAttachedStaticMesh")
        {
            return ESkeletalStaticChildImportAction::KeepAsAttachedStaticMesh;
        }

        if (ImportKind == "Ignore")
        {
            return ESkeletalStaticChildImportAction::Ignore;
        }

        return ESkeletalStaticChildImportAction::MergeAsRigidPart;
    }

    static bool IsExplicitIgnoredNode(FbxNode* Node)
    {
        return ReadStringProperty(Node, "ImportKind") == "Ignore";
    }

    static bool IsExplicitCollisionNode(FbxNode* Node)
    {
        return ReadStringProperty(Node, "ImportKind") == "Collision";
    }

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

    static FMatrix ComputeMeshLocalMatrixToBone(FbxNode* MeshNode, FbxNode* BoneNode)
    {
        if (!MeshNode || !BoneNode)
        {
            return FMatrix::Identity;
        }

        const FMatrix MeshGlobalScene = FFbxTransformUtils::ToEngineMatrix(FFbxTransformUtils::GetNodeGeometryTransform(MeshNode)) *
        FFbxTransformUtils::ToEngineMatrix(MeshNode->EvaluateGlobalTransform());

        const FMatrix BoneGlobalScene = FFbxTransformUtils::ToEngineMatrix(BoneNode->EvaluateGlobalTransform());

        return MeshGlobalScene * BoneGlobalScene.GetInverse();
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
        ImportNode.MeshNode       = MeshNode;
        ImportNode.SourceNodeName = MeshNode->GetName();

        int32      ParentBoneIndex = -1;
        FbxNode*   ParentBoneNode  = nullptr;
        const bool bHasParentBone  = FindNearestParentBoneIndex(MeshNode, BoneNodeToIndex, ParentBoneNode, ParentBoneIndex);

        if (IsExplicitIgnoredNode(MeshNode))
        {
            ImportNode.Kind = EFbxSkeletalImportMeshKind::Ignored;
            OutImportNodes.push_back(ImportNode);
            continue;
        }

        if (FFbxSceneQuery::MeshHasSkin(Mesh))
        {
            ImportNode.Kind = EFbxSkeletalImportMeshKind::Skinned;
            OutImportNodes.push_back(ImportNode);
            continue;
        }

        if (IsCollisionProxyName(ImportNode.SourceNodeName) || IsExplicitCollisionNode(MeshNode))
        {
            ImportNode.Kind                    = EFbxSkeletalImportMeshKind::CollisionProxy;
            ImportNode.ParentBoneIndex         = ParentBoneIndex;
            ImportNode.ParentBoneNode          = ParentBoneNode;
            ImportNode.LocalMatrixToParentBone = bHasParentBone ? ComputeMeshLocalMatrixToBone(MeshNode, ParentBoneNode) : FMatrix::Identity;
            OutImportNodes.push_back(ImportNode);
            continue;
        }

        if (bHasParentBone)
        {
            ImportNode.Kind                    = EFbxSkeletalImportMeshKind::StaticChildOfBone;
            ImportNode.ParentBoneIndex         = ParentBoneIndex;
            ImportNode.ParentBoneNode          = ParentBoneNode;
            ImportNode.LocalMatrixToParentBone = ComputeMeshLocalMatrixToBone(MeshNode, ParentBoneNode);
            ImportNode.StaticChildAction       = ReadStaticChildAction(MeshNode);
            OutImportNodes.push_back(ImportNode);
            continue;
        }

        ImportNode.Kind = EFbxSkeletalImportMeshKind::Loose;
        OutImportNodes.push_back(ImportNode);
    }
}
