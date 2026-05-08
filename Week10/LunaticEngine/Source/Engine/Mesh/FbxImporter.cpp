#include "Mesh/FbxImporter.h"

#include "Mesh/StaticMeshAsset.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Materials/MaterialManager.h"
#include "Engine/Platform/Paths.h"

#include <fbxsdk.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace
{
    void SetMessage(FString* OutMessage, const FString& Message)
    {
        if (OutMessage)
        {
            *OutMessage = Message;
        }
    }

    void SetMessage(FString* OutMessage, const char* Message)
    {
        if (OutMessage)
        {
            *OutMessage = Message ? Message : "";
        }
    }

    struct FFbxSceneHandle
    {
        FbxManager* Manager = nullptr;
        FbxScene*   Scene   = nullptr;

        ~FFbxSceneHandle()
        {
            if (Manager)
            {
                Manager->Destroy();
                Manager = nullptr;
                Scene   = nullptr;
            }
        }
    };

    struct FImportedBoneWeight
    {
        int32 BoneIndex = 0;
        float Weight    = 0.0f;
    };

    static bool LoadFbxScene(const FString& SourcePath, FFbxSceneHandle& OutScene, FString* OutMessage = nullptr)
    {
        const FString FullPath = FPaths::ConvertRelativePathToFull(SourcePath);

        FbxManager* Manager = FbxManager::Create();
        if (!Manager)
        {
            SetMessage(OutMessage, "Failed to create FBX SDK manager");
            return false;
        }

        FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
        if (!IOSettings)
        {
            SetMessage(OutMessage, "Failed to create FBX SDK IO settings");
            Manager->Destroy();
            return false;
        }

        Manager->SetIOSettings(IOSettings);

        FbxImporter* Importer = FbxImporter::Create(Manager, "");
        if (!Importer)
        {
            SetMessage(OutMessage, "Failed to create FBX SDK importer");
            Manager->Destroy();
            return false;
        }

        if (!Importer->Initialize(FullPath.c_str(), -1, Manager->GetIOSettings()))
        {
            FString Error = "FBX initialize failed: ";
            Error         += Importer->GetStatus().GetErrorString();

            SetMessage(OutMessage, Error);

            Importer->Destroy();
            Manager->Destroy();
            return false;
        }

        FbxScene* Scene = FbxScene::Create(Manager, "ImportScene");
        if (!Scene)
        {
            SetMessage(OutMessage, "Failed to create FBX SDK scene");

            Importer->Destroy();
            Manager->Destroy();
            return false;
        }

        if (!Importer->Import(Scene))
        {
            FString Error = "FBX import failed: ";
            Error         += Importer->GetStatus().GetErrorString();

            SetMessage(OutMessage, Error);

            Importer->Destroy();
            Manager->Destroy();
            return false;
        }

        Importer->Destroy();

        OutScene.Manager = Manager;
        OutScene.Scene   = Scene;

        return true;
    }

    static void TriangulateScene(FbxManager* Manager, FbxScene* Scene)
    {
        if (!Manager || !Scene)
        {
            return;
        }

        FbxGeometryConverter Converter(Manager);
        Converter.Triangulate(Scene, true);
    }

    static void CollectMeshNodes(FbxNode* Node, TArray<FbxNode*>& OutMeshNodes)
    {
        if (!Node)
        {
            return;
        }

        FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
        if (Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            OutMeshNodes.push_back(Node);
        }

        for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
        {
            CollectMeshNodes(Node->GetChild(ChildIndex), OutMeshNodes);
        }
    }

    static bool MeshHasSkin(FbxMesh* Mesh)
    {
        return Mesh && Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
    }

    static FMatrix ToEngineMatrix(const FbxAMatrix& Source)
    {
        FMatrix Result = FMatrix::Identity;

        for (int32 Row = 0; Row < 4; ++Row)
        {
            for (int32 Col = 0; Col < 4; ++Col)
            {
                Result.M[Row][Col] = static_cast<float>(Source.Get(Row, Col));
            }
        }

        return Result;
    }

    static FbxAMatrix GetNodeGeometryTransform(FbxNode* Node)
    {
        FbxAMatrix GeometryTransform;
        GeometryTransform.SetIdentity();

        if (!Node)
        {
            return GeometryTransform;
        }

        GeometryTransform.SetT(Node->GetGeometricTranslation(FbxNode::eSourcePivot));
        GeometryTransform.SetR(Node->GetGeometricRotation(FbxNode::eSourcePivot));
        GeometryTransform.SetS(Node->GetGeometricScaling(FbxNode::eSourcePivot));

        return GeometryTransform;
    }

    static bool IsNearlyZeroVector(const FVector& V, float Tolerance = 1.0e-6f)
    {
        return std::fabs(V.X) <= Tolerance && std::fabs(V.Y) <= Tolerance && std::fabs(V.Z) <= Tolerance;
    }

    static bool IsNearlyZeroVector4(const FVector4& V, float Tolerance = 1.0e-6f)
    {
        return std::fabs(V.X) <= Tolerance && std::fabs(V.Y) <= Tolerance && std::fabs(V.Z) <= Tolerance && std::fabs(V.W) <= Tolerance;
    }

    static bool ContainsFbxNode(const TArray<FbxNode*>& Nodes, const FbxNode* Node)
    {
        return std::find(Nodes.begin(), Nodes.end(), Node) != Nodes.end();
    }

    static void AddUniqueFbxNode(TArray<FbxNode*>& Nodes, FbxNode* Node)
    {
        if (Node && !ContainsFbxNode(Nodes, Node))
        {
            Nodes.push_back(Node);
        }
    }

    static void CollectSkinClusterLinksFromMesh(FbxMesh* Mesh, TArray<FbxNode*>& OutLinkNodes)
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

                AddUniqueFbxNode(OutLinkNodes, Cluster->GetLink());
            }
        }
    }

    static bool IsSceneRootNode(FbxNode* Node)
    {
        return Node && Node->GetParent() == nullptr;
    }

    static void AddNodeAndParentsUntilSceneRoot(FbxNode* Node, TArray<FbxNode*>& OutNodes)
    {
        FbxNode* Current = Node;

        while (Current && !IsSceneRootNode(Current))
        {
            AddUniqueFbxNode(OutNodes, Current);
            Current = Current->GetParent();
        }
    }

    static void FindImportedBoneRoot(const TArray<FbxNode*>& Nodes, TArray<FbxNode*>& OutRoots)
    {
        for (FbxNode* Node : Nodes)
        {
            if (!Node)
            {
                continue;
            }

            FbxNode* Parent = Node->GetParent();

            if (!Parent || !ContainsFbxNode(Nodes, Parent))
            {
                AddUniqueFbxNode(OutRoots, Node);
            }
        }
    }

    static int32 AddImportedBoneRecursive(FbxNode* BoneNode, int32 ParentIndex, const TArray<FbxNode*>& ImportedBoneNodes, FSkeleton& OutSkeleton, TMap<FbxNode*, int32>& OutBoneNodeToIndex)
    {
        if (!BoneNode || !ContainsFbxNode(ImportedBoneNodes, BoneNode))
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

        Bone.LocalBindPose   = ToEngineMatrix(LocalBind);
        Bone.GlobalBindPose  = ToEngineMatrix(GlobalBind);
        Bone.InverseBindPose = Bone.GlobalBindPose.GetInverse();

        OutSkeleton.Bones.push_back(Bone);
        OutBoneNodeToIndex[BoneNode] = BoneIndex;

        for (int32 ChildIndex = 0; ChildIndex < BoneNode->GetChildCount(); ++ChildIndex)
        {
            FbxNode* Child = BoneNode->GetChild(ChildIndex);

            if (ContainsFbxNode(ImportedBoneNodes, Child))
            {
                AddImportedBoneRecursive(Child, BoneIndex, ImportedBoneNodes, OutSkeleton, OutBoneNodeToIndex);
            }
        }
        return BoneIndex;
    }

    static bool BuildSkeletonFromSkinClusters(const TArray<FbxNode*>& SkinnedMeshNodes, FSkeleton& OutSkeleton, TMap<FbxNode*, int32>& OutBoneNodeToIndex)
    {
        OutSkeleton.Bones.clear();
        OutBoneNodeToIndex.clear();

        TArray<FbxNode*> LinkNodes;

        for (FbxNode* MeshNode : SkinnedMeshNodes)
        {
            FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

            if (Mesh && MeshHasSkin(Mesh))
            {
                CollectSkinClusterLinksFromMesh(Mesh, LinkNodes);
            }
        }

        if (LinkNodes.empty())
        {
            return false;
        }

        TArray<FbxNode*> ImportedBoneNodes;

        for (FbxNode* LinkNode : LinkNodes)
        {
            AddNodeAndParentsUntilSceneRoot(LinkNode, ImportedBoneNodes);
        }

        TArray<FbxNode*> RootBones;

        FindImportedBoneRoot(ImportedBoneNodes, RootBones);

        for (FbxNode* RootBone : RootBones)
        {
            AddImportedBoneRecursive(RootBone, -1, ImportedBoneNodes, OutSkeleton, OutBoneNodeToIndex);
        }

        OutSkeleton.RebuildChildren();

        return !OutSkeleton.Bones.empty();
    }

    static int32 ParseLODIndexFromName(const FString& Name)
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

    static int32 GetSkeletalMeshLODIndex(FbxNode* MeshNode)
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
    
    static void GetOrCreateDefaultMaterial(TArray<FStaticMaterial>& OutMaterials)
    {
        if (!OutMaterials.empty())
        {
            return;
        }

        FStaticMaterial DefaultMaterial;
        DefaultMaterial.MaterialSlotName  = "None";
        DefaultMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");

        OutMaterials.push_back(DefaultMaterial);
    }

    static const char* GetPrimaryUVSetName(FbxMesh* Mesh, FbxStringList& OutUVSetNames)
    {
        if (!Mesh)
        {
            return nullptr;
        }

        Mesh->GetUVSetNames(OutUVSetNames);

        if (OutUVSetNames.GetCount() <= 0)
        {
            return nullptr;
        }

        return OutUVSetNames[0];
    }

    static FVector ReadPosition(FbxMesh* Mesh, int32 ControlPointIndex)
    {
        if (!Mesh) return FVector(0.0f, 0.0f, 0.0f);

        const int32 ControlPointCount = Mesh->GetControlPointsCount();

        if (ControlPointIndex < 0 || ControlPointIndex >= ControlPointCount) return FVector(0.0f, 0.0f, 0.0f);

        FbxVector4*      ControlPoint = Mesh->GetControlPoints();
        const FbxVector4 P            = ControlPoint[ControlPointIndex];

        return FVector(static_cast<float>(P[0]), static_cast<float>(P[1]), static_cast<float>(P[2]));
    }

    static FVector ReadNormal(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex)
    {
        FbxVector4 Normal;

        if (Mesh && Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, Normal))
        {
            return FVector(static_cast<float>(Normal[0]), static_cast<float>(Normal[1]), static_cast<float>(Normal[2])).Normalized();
        }

        return FVector(0.0f, 0.0f, 1.0f);
    }

    static FVector2 ReadUV(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex)
    {
        if (!Mesh)
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxStringList UVSetNames;
        const char*   UVSetName = GetPrimaryUVSetName(Mesh, UVSetNames);

        if (!UVSetName)
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxVector2 UV;
        bool       bUnmapped = false;

        if (Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetName, UV, bUnmapped))
        {
            return FVector2(static_cast<float>(UV[0]), 1.0f - static_cast<float>(UV[1]));
        }

        return FVector2(0.0f, 0.0f);
    }

    static int32 GetUVSetCount(FbxMesh* Mesh)
    {
        if (!Mesh)
        {
            return 0;
        }

        FbxStringList UVSetNames;
        Mesh->GetUVSetNames(UVSetNames);
        return static_cast<int32>(UVSetNames.GetCount());
    }

    static FVector2 ReadUVByChannel(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex, int32 ChannelIndex)
    {
        if (!Mesh)
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxStringList UVSetNames;
        Mesh->GetUVSetNames(UVSetNames);

        if (ChannelIndex < 0 || ChannelIndex >= UVSetNames.GetCount())
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxVector2 UV;
        bool       bUnmapped = false;

        if (Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetNames[ChannelIndex], UV, bUnmapped) && !bUnmapped)
        {
            return FVector2(static_cast<float>(UV[0]), 1.0f - static_cast<float>(UV[1]));
        }

        return FVector2(0.0f, 0.0f);
    }

    static FVector4 ReadVertexColor(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonVertexIndex)
    {
        if (!Mesh || !Mesh->GetLayer(0))
        {
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        FbxLayerElementVertexColor* ColorElement = Mesh->GetLayer(0)->GetVertexColors();
        if (!ColorElement)
        {
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        int32 ColorIndex = 0;

        switch (ColorElement->GetMappingMode())
        {
        case FbxLayerElement::eByControlPoint:
            ColorIndex = ControlPointIndex;
            break;

        case FbxLayerElement::eByPolygonVertex:
            ColorIndex = PolygonVertexIndex;
            break;

        case FbxLayerElement::eAllSame:
            ColorIndex = 0;
            break;

        default:
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if (ColorIndex < 0)
        {
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if (ColorElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
        {
            if (ColorIndex >= ColorElement->GetIndexArray().GetCount())
            {
                return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
            }

            ColorIndex = ColorElement->GetIndexArray().GetAt(ColorIndex);
        }

        if (ColorIndex < 0 || ColorIndex >= ColorElement->GetDirectArray().GetCount())
        {
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        const FbxColor C = ColorElement->GetDirectArray().GetAt(ColorIndex);

        return FVector4(static_cast<float>(C.mRed), static_cast<float>(C.mGreen), static_cast<float>(C.mBlue), static_cast<float>(C.mAlpha));
    }

    static bool TryReadTangent(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector4& OutTangent)
    {
        if (!Mesh || !Mesh->GetLayer(0))
        {
            return false;
        }

        FbxLayerElementTangent* TangentElement = Mesh->GetLayer(0)->GetTangents();
        if (!TangentElement)
        {
            return false;
        }

        int32 TangentIndex = 0;

        switch (TangentElement->GetMappingMode())
        {
        case FbxLayerElement::eByControlPoint:
            TangentIndex = ControlPointIndex;
            break;

        case FbxLayerElement::eByPolygonVertex:
            TangentIndex = PolygonVertexIndex;
            break;

        case FbxLayerElement::eAllSame:
            TangentIndex = 0;
            break;

        default:
            return false;
        }

        if (TangentIndex < 0)
        {
            return false;
        }

        if (TangentElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
        {
            if (TangentIndex >= TangentElement->GetIndexArray().GetCount())
            {
                return false;
            }

            TangentIndex = TangentElement->GetIndexArray().GetAt(TangentIndex);
        }

        if (TangentIndex < 0 || TangentIndex >= TangentElement->GetDirectArray().GetCount())
        {
            return false;
        }

        const FbxVector4 T = TangentElement->GetDirectArray().GetAt(TangentIndex);

        FVector Tangent(static_cast<float>(T[0]), static_cast<float>(T[1]), static_cast<float>(T[2]));

        Tangent = Tangent.Normalized();

        const float W = (std::fabs(static_cast<float>(T[3])) > 1.0e-6f) ? static_cast<float>(T[3]) : 1.0f;
        OutTangent    = FVector4(Tangent.X, Tangent.Y, Tangent.Z, W);

        return true;
    }

    template <typename LayerElementType>
    static bool TryGetLayerElementVector4(LayerElementType* Element, int32 ControlPointIndex, int32 PolygonVertexIndex, FbxVector4& OutValue)
    {
        if (!Element)
        {
            return false;
        }

        int32 ElementIndex = 0;

        switch (Element->GetMappingMode())
        {
        case FbxLayerElement::eByControlPoint:
            ElementIndex = ControlPointIndex;
            break;

        case FbxLayerElement::eByPolygonVertex:
            ElementIndex = PolygonVertexIndex;
            break;

        case FbxLayerElement::eAllSame:
            ElementIndex = 0;
            break;

        default:
            return false;
        }

        if (ElementIndex < 0)
        {
            return false;
        }

        if (Element->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
        {
            if (ElementIndex >= Element->GetIndexArray().GetCount())
            {
                return false;
            }

            ElementIndex = Element->GetIndexArray().GetAt(ElementIndex);
        }

        if (ElementIndex < 0 || ElementIndex >= Element->GetDirectArray().GetCount())
        {
            return false;
        }

        OutValue = Element->GetDirectArray().GetAt(ElementIndex);
        return true;
    }

    static bool TryReadShapeNormal(FbxShape* Shape, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector& OutNormal)
    {
        if (!Shape || !Shape->GetLayer(0))
        {
            return false;
        }

        FbxVector4 Value;
        if (!TryGetLayerElementVector4(Shape->GetLayer(0)->GetNormals(), ControlPointIndex, PolygonVertexIndex, Value))
        {
            return false;
        }

        OutNormal = FVector(static_cast<float>(Value[0]), static_cast<float>(Value[1]), static_cast<float>(Value[2])).Normalized();
        return true;
    }

    static bool TryReadShapeTangent(FbxShape* Shape, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector4& OutTangent)
    {
        if (!Shape || !Shape->GetLayer(0))
        {
            return false;
        }

        FbxVector4 Value;
        if (!TryGetLayerElementVector4(Shape->GetLayer(0)->GetTangents(), ControlPointIndex, PolygonVertexIndex, Value))
        {
            return false;
        }

        FVector Tangent(static_cast<float>(Value[0]), static_cast<float>(Value[1]), static_cast<float>(Value[2]));
        Tangent = Tangent.Normalized();

        const float W = (std::fabs(static_cast<float>(Value[3])) > 1.0e-6f) ? static_cast<float>(Value[3]) : 1.0f;
        OutTangent    = FVector4(Tangent.X, Tangent.Y, Tangent.Z, W);
        return true;
    }

    static FVector ComputeTriangleTangent(const FVector& P0, const FVector& P1, const FVector& P2, const FVector2& UV0, const FVector2& UV1, const FVector2& UV2)
    {
        const FVector Edge1 = P1 - P0;
        const FVector Edge2 = P2 - P0;

        const float DU1 = UV1.X - UV0.X;
        const float DU2 = UV2.X - UV0.X;
        const float DV1 = UV1.Y - UV0.Y;
        const float DV2 = UV2.Y - UV0.Y;

        const float Denom = DU1 * DV2 - DU2 * DV1;

        if (std::fabs(Denom) <= 1e-6f)
        {
            return FVector(1.0f, 0.0f, 0.0f);
        }

        const float R       = 1.0f / Denom;
        FVector     Tangent = (Edge1 * DV2 - Edge2 * DV1) * R;

        return Tangent.Normalized();
    }

    static void AddBoneWeight(TArray<FImportedBoneWeight>& Weights, int32 BoneIndex, float Weight)
    {
        if (Weight <= 0.0f)
        {
            return;
        }

        for (FImportedBoneWeight& Existing : Weights)
        {
            if (Existing.BoneIndex == BoneIndex)
            {
                Existing.Weight += Weight;
                return;
            }
        }

        FImportedBoneWeight NewWeight;
        NewWeight.BoneIndex = BoneIndex;
        NewWeight.Weight    = Weight;
        Weights.push_back(NewWeight);
    }

    struct FImportedMorphSourceVertex
    {
        FbxMesh* Mesh               = nullptr;
        int32    ControlPointIndex  = -1;
        int32    PolygonIndex       = -1;
        int32    CornerIndex        = -1;
        int32    PolygonVertexIndex = -1;
        uint32   VertexIndex        = 0;
        FMatrix  MeshToReference;
        FMatrix  NormalToReference;
        FVector  BaseNormalInReference;
        FVector4 BaseTangentInReference;
    };

    static FVector TransformPositionByMatrix(const FVector& P, const FMatrix& M)
    {
        return P * M;
    }

    static FVector TransformDirectionByMatrix(const FVector& V, const FMatrix& M)
    {
        return M.TransformVector(V).Normalized();
    }

    static FVector TransformNormalByMatrix(const FVector& V, const FMatrix& NormalMatrix)
    {
        return NormalMatrix.TransformVector(V).Normalized();
    }

    static FVector OrthogonalizeTangentToNormal(const FVector& Tangent, const FVector& Normal)
    {
        const FVector N = Normal.Normalized();

        FVector T = Tangent - (N * Tangent.Dot(N));

        if (T.IsNearlyZero(1.0e-6f))
        {
            const FVector Candidate = (std::fabs(N.Z) < 0.999f) ? FVector::UpVector : FVector::RightVector;

            T = Candidate - (N * Candidate.Dot(N));
        }

        return T.Normalized();
    }

    static FVector TransformTangentByMatrix(const FVector& Tangent, const FMatrix& TangentMatrix, const FVector& ReferenceNormal)
    {
        const FVector ReferenceTangent = TransformDirectionByMatrix(Tangent, TangentMatrix);
        return OrthogonalizeTangentToNormal(ReferenceTangent, ReferenceNormal);
    }

    static FVector TransformVectorNoNormalizeByMatrix(const FVector& V, const FMatrix& M)
    {
        return M.TransformVector(V);
    }

    static FMatrix RemoveScaleFromMatrix(const FMatrix& Matrix)
    {
        FVector XAxis(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
        FVector YAxis(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
        FVector ZAxis(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);

        XAxis = XAxis.Normalized();
        YAxis = YAxis.Normalized();
        ZAxis = ZAxis.Normalized();

        FMatrix Result = FMatrix::Identity;

        Result.M[0][0] = XAxis.X;
        Result.M[0][1] = XAxis.Y;
        Result.M[0][2] = XAxis.Z;

        Result.M[1][0] = YAxis.X;
        Result.M[1][1] = YAxis.Y;
        Result.M[1][2] = YAxis.Z;

        Result.M[2][0] = ZAxis.X;
        Result.M[2][1] = ZAxis.Y;
        Result.M[2][2] = ZAxis.Z;

        return Result;
    }

    static FBoneTransformKey MakeBoneTransformKeyFromEngineMatrix(float TimeSeconds, const FMatrix& LocalMatrix)
    {
        FBoneTransformKey Key;
        Key.TimeSeconds = TimeSeconds;
        Key.Translation = LocalMatrix.GetLocation();
        Key.Scale       = LocalMatrix.GetScale();
        Key.Rotation    = RemoveScaleFromMatrix(LocalMatrix).ToQuat().GetNormalized();
        return Key;
    }

    static bool TryGetFirstMeshBindMatrix(FbxNode* MeshNode, FbxAMatrix& OutMeshBindMatrix)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
        if (!Mesh)
        {
            return false;
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

                FbxAMatrix MeshNodeBind;
                Cluster->GetTransformMatrix(MeshNodeBind);

                // FBX geometric transforms are not inherited by child nodes, but they do affect
                // control-point positions.  Keep mesh vertices, morph deltas and the reference bind
                // space in the same geometric bind space.
                OutMeshBindMatrix = GetNodeGeometryTransform(MeshNode) * MeshNodeBind;
                return true;
            }
        }

        return false;
    }

    static bool TryGetReferenceMeshBindMatrix(const TArray<FbxNode*>& SkinnedMeshNodes, FbxAMatrix& OutReferenceMeshBindMatrix)
    {
        for (FbxNode* MeshNode : SkinnedMeshNodes)
        {
            if (TryGetFirstMeshBindMatrix(MeshNode, OutReferenceMeshBindMatrix))
            {
                return true;
            }
        }

        return false;
    }

    static void ExtractSkinWeightsOnly(FbxMesh* Mesh, const TMap<FbxNode*, int32>& BoneNodeToIndex, TArray<TArray<FImportedBoneWeight>>& OutControlPointWeight)
    {
        OutControlPointWeight.clear();

        if (!Mesh)
        {
            return;
        }

        const int32 ControlPointCount = Mesh->GetControlPointsCount();
        OutControlPointWeight.resize(ControlPointCount);

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

                const int32*  ControlPointIndices = Cluster->GetControlPointIndices();
                const double* ControlPointWeights = Cluster->GetControlPointWeights();
                const int32   InfluenceCount      = Cluster->GetControlPointIndicesCount();

                for (int32 InfluenceIndex = 0; InfluenceIndex < InfluenceCount; ++InfluenceIndex)
                {
                    const int32 ControlPointIndex = ControlPointIndices[InfluenceIndex];
                    const float Weight            = static_cast<float>(ControlPointWeights[InfluenceIndex]);

                    if (ControlPointIndex < 0 || ControlPointIndex >= ControlPointCount)
                    {
                        continue;
                    }

                    AddBoneWeight(OutControlPointWeight[ControlPointIndex], BoneIndex, Weight);
                }
            }
        }
    }

    static void InitializeBoneBindPoseFromSceneNodes(const TMap<FbxNode*, int32>& BoneNodeToIndex, const FMatrix& ReferenceMeshBindInverse, FSkeleton& Skeleton)
    {
        for (const auto& Pair : BoneNodeToIndex)
        {
            FbxNode*    BoneNode  = Pair.first;
            const int32 BoneIndex = Pair.second;

            if (!BoneNode || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
            {
                continue;
            }

            const FMatrix BoneGlobal = ToEngineMatrix(BoneNode->EvaluateGlobalTransform());

            const FMatrix BoneInReferenceMeshSpace = BoneGlobal * ReferenceMeshBindInverse;

            Skeleton.Bones[BoneIndex].GlobalBindPose  = BoneInReferenceMeshSpace;
            Skeleton.Bones[BoneIndex].InverseBindPose = BoneInReferenceMeshSpace.GetInverse();
        }
    }

    static void ApplyBindPoseFromSkinClusters(const TArray<FbxNode*>& SkinnedMeshNodes, const TMap<FbxNode*, int32>& BoneNodeToIndex, const FMatrix& ReferenceMeshBindInverse, FSkeleton& Skeleton, TArray<bool>* InOutAppliedBoneMask = nullptr)
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

                    if (InOutAppliedBoneMask && BoneIndex < static_cast<int32>(InOutAppliedBoneMask->size()) && (*InOutAppliedBoneMask)[BoneIndex])
                    {
                        continue;
                    }

                    FbxAMatrix LinkBindFbx;
                    Cluster->GetTransformLinkMatrix(LinkBindFbx);

                    const FMatrix LinkBind = ToEngineMatrix(LinkBindFbx);

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

    static void PackTop4Weights(const TArray<FImportedBoneWeight>& SourceWeight, uint16 OutBoneIndices[4], float OutBoneWeights[4])
    {
        for (int32 i = 0; i < 4; ++i)
        {
            OutBoneIndices[i] = 0;
            OutBoneWeights[i] = 0.0f;
        }

        if (SourceWeight.empty())
        {
            OutBoneIndices[0] = 0;
            OutBoneWeights[0] = 1.0f;
            return;
        }

        TArray<FImportedBoneWeight> SortedWeights = SourceWeight;
        std::sort(SortedWeights.begin(), SortedWeights.end(), [](const FImportedBoneWeight& A, const FImportedBoneWeight& B)
        {
            return A.Weight > B.Weight;
        });

        const int32 Count = static_cast<int32>(std::min<size_t>(4, SortedWeights.size()));

        float Sum = 0.0f;

        for (int32 i = 0; i < Count; ++i)
        {
            OutBoneIndices[i] = static_cast<uint16>(SortedWeights[i].BoneIndex);
            OutBoneWeights[i] = SortedWeights[i].Weight;
            Sum               += OutBoneWeights[i];
        }

        if (Sum <= 1e-6f)
        {
            OutBoneIndices[0] = 0;
            OutBoneWeights[0] = 1.0f;

            for (int32 i = 1; i < 4; ++i)
            {
                OutBoneIndices[i] = 0;
                OutBoneWeights[i] = 0.0f;
            }
            return;
        }

        for (int32 i = 0; i < Count; ++i)
        {
            OutBoneWeights[i] /= Sum;
        }
    }

    struct FImportedSectionBuild
    {
        int32          MaterialIndex = 0;
        TArray<uint32> Indices;
    };

    static FImportedSectionBuild* FindOrAddImportedSection(TArray<FImportedSectionBuild>& Sections, int32 MaterialIndex)
    {
        for (FImportedSectionBuild& Section : Sections)
        {
            if (Section.MaterialIndex == MaterialIndex)
            {
                return &Section;
            }
        }

        FImportedSectionBuild NewSectionBuild;
        NewSectionBuild.MaterialIndex = MaterialIndex;
        Sections.push_back(NewSectionBuild);
        return &Sections.back();
    }

    static int32 GetPolygonMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex)
    {
        if (!Mesh || !Mesh->GetLayer(0))
        {
            return 0;
        }

        FbxLayerElementMaterial* MaterialElement = Mesh->GetLayer(0)->GetMaterials();
        if (!MaterialElement)
        {
            return 0;
        }

        if (MaterialElement->GetMappingMode() == FbxLayerElement::eByPolygon)
        {
            if (MaterialElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect || MaterialElement->GetReferenceMode() == FbxLayerElement::eIndex)
            {
                if (PolygonIndex >= 0 && PolygonIndex < MaterialElement->GetIndexArray().GetCount())
                {
                    return MaterialElement->GetIndexArray().GetAt(PolygonIndex);
                }
            }
            return 0;
        }
        if (MaterialElement->GetMappingMode() == FbxLayerElement::eAllSame)
        {
            if (MaterialElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect || MaterialElement->GetReferenceMode() == FbxLayerElement::eIndex)
            {
                if (MaterialElement->GetIndexArray().GetCount() > 0)
                {
                    return MaterialElement->GetIndexArray().GetAt(0);
                }
            }

            return 0;
        }

        return 0;
    }

    static int32 FindOrAddSkeletalMaterial(FbxSurfaceMaterial* FbxMaterial, TArray<FStaticMaterial>& OutMaterials)
    {
        const FString SlotName = FbxMaterial ? FbxMaterial->GetName() : "None";

        for (int32 i = 0; i < static_cast<int32>(OutMaterials.size()); ++i)
        {
            if (OutMaterials[i].MaterialSlotName == SlotName)
            {
                return i;
            }
        }

        FStaticMaterial NewMaterial;
        NewMaterial.MaterialSlotName  = SlotName;
        NewMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(SlotName);
        OutMaterials.push_back(NewMaterial);
        return static_cast<int32>(OutMaterials.size()) - 1;
    }

    static void BuildFinalSkeletalSections(const TArray<FImportedSectionBuild>& SectionBuilds, const TArray<FStaticMaterial>& Materials, TArray<uint32>& OutIndices, TArray<FStaticMeshSection>& OutSections)
    {
        OutSections.clear();
        OutIndices.clear();

        for (const FImportedSectionBuild& Build : SectionBuilds)
        {
            if (Build.Indices.empty())
            {
                continue;
            }

            FStaticMeshSection Section;
            Section.MaterialIndex    = Build.MaterialIndex;
            Section.MaterialSlotName = Materials[Build.MaterialIndex].MaterialSlotName;
            Section.FirstIndex       = static_cast<uint32>(OutIndices.size());
            Section.NumTriangles     = static_cast<uint32>(Build.Indices.size() / 3);

            for (uint32 Index : Build.Indices)
            {
                OutIndices.push_back(Index);
            }

            OutSections.push_back(Section);
        }
    }

    static void RecomputeLocalBindPose(FSkeleton& Skeleton)
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
        }
        Skeleton.RebuildChildren();
    }

    static void NormalizeFbxScene(FbxScene* Scene)
    {
        if (!Scene)
        {
            return;
        }

        FbxAxisSystem EngineAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);

        const FbxAxisSystem SceneAxisSystem = Scene->GetGlobalSettings().GetAxisSystem();

        if (SceneAxisSystem != EngineAxisSystem)
        {
            EngineAxisSystem.ConvertScene(Scene);
        }

        const FbxSystemUnit EngineUnit = FbxSystemUnit::cm;
        const FbxSystemUnit SceneUnit  = Scene->GetGlobalSettings().GetSystemUnit();

        if (SceneUnit != EngineUnit)
        {
            EngineUnit.ConvertScene(Scene);
        }
    }

    static void ImportAnimations(FbxScene* Scene, const TMap<FbxNode*, int32>& BoneNodeToIndex, const FMatrix& ReferenceMeshBindInverse, const FSkeleton& Skeleton, TArray<FSkeletalAnimationClip>& OutAnimations)
    {
        OutAnimations.clear();

        if (!Scene)
        {
            return;
        }

        const int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
        if (AnimStackCount <= 0)
        {
            return;
        }

        const float SampleRate = 30.0f;

        for (int32 StackIndex = 0; StackIndex < AnimStackCount; ++StackIndex)
        {
            FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
            if (!AnimStack)
            {
                continue;
            }

            Scene->SetCurrentAnimationStack(AnimStack);

            FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();

            const double StartSeconds = TimeSpan.GetStart().GetSecondDouble();
            const double EndSeconds   = TimeSpan.GetStop().GetSecondDouble();

            if (EndSeconds <= StartSeconds)
            {
                continue;
            }

            FSkeletalAnimationClip Clip;
            Clip.Name            = AnimStack->GetName();
            Clip.DurationSeconds = static_cast<float>(EndSeconds - StartSeconds);
            Clip.SampleRate      = SampleRate;

            Clip.Tracks.resize(Skeleton.Bones.size());

            for (const auto& Pair : BoneNodeToIndex)
            {
                const int32 BoneIndex = Pair.second;

                if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
                {
                    continue;
                }

                FBoneAnimationTrack& Track = Clip.Tracks[BoneIndex];
                Track.BoneIndex            = BoneIndex;
                Track.BoneName             = Skeleton.Bones[BoneIndex].Name;
            }

            const double DurationSeconds = static_cast<double>(Clip.DurationSeconds);
            const int32  WholeFrameCount = static_cast<int32>(std::floor(DurationSeconds * static_cast<double>(SampleRate) + 1.0e-6));

            auto AddAnimationKeysAtTime = [&](double LocalSeconds)
            {
                if (LocalSeconds < 0.0)
                {
                    LocalSeconds = 0.0;
                }
                else if (LocalSeconds > DurationSeconds)
                {
                    LocalSeconds = DurationSeconds;
                }

                const double AbsoluteSeconds = StartSeconds + LocalSeconds;

                FbxTime Time;
                Time.SetSecondDouble(AbsoluteSeconds);

                TArray<FMatrix> GlobalInReference;
                GlobalInReference.resize(Skeleton.Bones.size());

                for (const auto& Pair : BoneNodeToIndex)
                {
                    FbxNode*    BoneNode  = Pair.first;
                    const int32 BoneIndex = Pair.second;

                    if (!BoneNode || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
                    {
                        continue;
                    }

                    const FMatrix BoneGlobal     = ToEngineMatrix(BoneNode->EvaluateGlobalTransform(Time));
                    GlobalInReference[BoneIndex] = BoneGlobal * ReferenceMeshBindInverse;
                }

                for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
                {
                    if (BoneIndex >= static_cast<int32>(Clip.Tracks.size()))
                    {
                        continue;
                    }

                    FMatrix     LocalMatrix = GlobalInReference[BoneIndex];
                    const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

                    if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(Skeleton.Bones.size()))
                    {
                        LocalMatrix = GlobalInReference[BoneIndex] * GlobalInReference[ParentIndex].GetInverse();
                    }

                    FBoneTransformKey Key = MakeBoneTransformKeyFromEngineMatrix(static_cast<float>(LocalSeconds), LocalMatrix);
                    Clip.Tracks[BoneIndex].Keys.push_back(Key);
                }
            };

            for (int32 FrameIndex = 0; FrameIndex <= WholeFrameCount; ++FrameIndex)
            {
                AddAnimationKeysAtTime(static_cast<double>(FrameIndex) / static_cast<double>(SampleRate));
            }

            const double LastWholeFrameSeconds = static_cast<double>(WholeFrameCount) / static_cast<double>(SampleRate);
            if (DurationSeconds - LastWholeFrameSeconds > 1.0e-4)
            {
                AddAnimationKeysAtTime(DurationSeconds);
            }

            OutAnimations.push_back(std::move(Clip));
        }
    }

    static void ImportMorphTargets(const TArray<TArray<FImportedMorphSourceVertex>>& MorphSourcesByLOD, TArray<FMorphTarget>& OutMorphTargets)
    {
        OutMorphTargets.clear();

        TMap<FString, int32> MorphNameToIndex;

        for (int32 LODIndex = 0; LODIndex < static_cast<int32>(MorphSourcesByLOD.size()); ++LODIndex)
        {
            const TArray<FImportedMorphSourceVertex>& Sources = MorphSourcesByLOD[LODIndex];

            TMap<FbxMesh*, TArray<const FImportedMorphSourceVertex*>> SourcesByMesh;

            for (const FImportedMorphSourceVertex& Source : Sources)
            {
                if (Source.Mesh)
                {
                    SourcesByMesh[Source.Mesh].push_back(&Source);
                }
            }

            for (const auto& MeshPair : SourcesByMesh)
            {
                FbxMesh*                                         Mesh        = MeshPair.first;
                const TArray<const FImportedMorphSourceVertex*>& MeshSources = MeshPair.second;

                if (!Mesh)
                {
                    continue;
                }

                const int32 BlendShapeCount = Mesh->GetDeformerCount(FbxDeformer::eBlendShape);

                for (int32 BlendShapeIndex = 0; BlendShapeIndex < BlendShapeCount; ++BlendShapeIndex)
                {
                    FbxBlendShape* BlendShape = static_cast<FbxBlendShape*>(Mesh->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape));

                    if (!BlendShape)
                    {
                        continue;
                    }

                    const int32 ChannelCount = BlendShape->GetBlendShapeChannelCount();

                    for (int32 ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
                    {
                        FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

                        if (!Channel)
                        {
                            continue;
                        }

                        const int32 TargetShapeCount = Channel->GetTargetShapeCount();

                        if (TargetShapeCount <= 0)
                        {
                            continue;
                        }

                        FbxShape* Shape = Channel->GetTargetShape(TargetShapeCount - 1);

                        if (!Shape)
                        {
                            continue;
                        }

                        FString MorphName = Channel->GetName();

                        if (MorphName.empty())
                        {
                            MorphName = Shape->GetName();
                        }

                        int32 MorphIndex = -1;

                        auto Existing = MorphNameToIndex.find(MorphName);
                        if (Existing != MorphNameToIndex.end())
                        {
                            MorphIndex = Existing->second;
                        }
                        else
                        {
                            FMorphTarget NewMorph;
                            NewMorph.Name = MorphName;
                            NewMorph.LODModels.resize(MorphSourcesByLOD.size());

                            OutMorphTargets.push_back(std::move(NewMorph));

                            MorphIndex                  = static_cast<int32>(OutMorphTargets.size()) - 1;
                            MorphNameToIndex[MorphName] = MorphIndex;
                        }

                        FMorphTargetLOD& MorphLOD = OutMorphTargets[MorphIndex].LODModels[LODIndex];

                        FbxVector4* BaseControlPoints   = Mesh->GetControlPoints();
                        FbxVector4* TargetControlPoints = Shape->GetControlPoints();

                        if (!BaseControlPoints || !TargetControlPoints)
                        {
                            continue;
                        }

                        const int32 ControlPointCount = Mesh->GetControlPointsCount();

                        for (const FImportedMorphSourceVertex* Source : MeshSources)
                        {
                            if (!Source)
                            {
                                continue;
                            }

                            const int32 CPIndex = Source->ControlPointIndex;

                            if (CPIndex < 0 || CPIndex >= ControlPointCount)
                            {
                                continue;
                            }

                            const FbxVector4 BaseP   = BaseControlPoints[CPIndex];
                            const FbxVector4 TargetP = TargetControlPoints[CPIndex];

                            FVector LocalDelta(static_cast<float>(TargetP[0] - BaseP[0]), static_cast<float>(TargetP[1] - BaseP[1]), static_cast<float>(TargetP[2] - BaseP[2]));

                            FMorphTargetDelta Delta;
                            Delta.VertexIndex   = Source->VertexIndex;
                            Delta.PositionDelta = TransformVectorNoNormalizeByMatrix(LocalDelta, Source->MeshToReference);
                            Delta.NormalDelta   = FVector(0.0f, 0.0f, 0.0f);
                            Delta.TangentDelta  = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

                            FVector TargetNormalInReference = Source->BaseNormalInReference;

                            FVector TargetLocalNormal;
                            if (TryReadShapeNormal(Shape, CPIndex, Source->PolygonVertexIndex, TargetLocalNormal))
                            {
                                TargetNormalInReference = TransformNormalByMatrix(TargetLocalNormal, Source->NormalToReference);
                                Delta.NormalDelta = TargetNormalInReference - Source->BaseNormalInReference;
                            }

                            FVector4 TargetLocalTangent;
                            if (TryReadShapeTangent(Shape, CPIndex, Source->PolygonVertexIndex, TargetLocalTangent))
                            {
                                const FVector TargetTangentInReference = TransformTangentByMatrix(FVector(TargetLocalTangent.X, TargetLocalTangent.Y, TargetLocalTangent.Z), Source->MeshToReference, TargetNormalInReference);
                                Delta.TangentDelta = FVector4(TargetTangentInReference.X - Source->BaseTangentInReference.X, TargetTangentInReference.Y - Source->BaseTangentInReference.Y, TargetTangentInReference.Z - Source->BaseTangentInReference.Z, TargetLocalTangent.W - Source->BaseTangentInReference.W);
                            }

                            if (IsNearlyZeroVector(Delta.PositionDelta) && IsNearlyZeroVector(Delta.NormalDelta) && IsNearlyZeroVector4(Delta.TangentDelta))
                            {
                                continue;
                            }

                            MorphLOD.Deltas.push_back(Delta);
                        }
                    }
                }
            }
        }
    }
}

static bool BuildSkeletalMeshLODFromNodes(const TArray<FbxNode*>& MeshNodes, const TMap<FbxNode*, int32>& BoneNodeToIndex, const FMatrix& ReferenceMeshBindInverse, TArray<FStaticMaterial>& OutMaterials, FSkeletalMeshLOD& OutLOD, TArray<FImportedMorphSourceVertex>* OutMorphSources)
{
    OutLOD = FSkeletalMeshLOD();

    if (OutMorphSources)
    {
        OutMorphSources->clear();
    }

    TArray<FImportedSectionBuild> SectionBuilds;

    for (FbxNode* MeshNode : MeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (!Mesh)
        {
            continue;
        }

        FbxAMatrix MeshBindFbx;
        if (!TryGetFirstMeshBindMatrix(MeshNode, MeshBindFbx))
        {
            continue;
        }

        const FMatrix MeshBind          = ToEngineMatrix(MeshBindFbx);
        const FMatrix MeshToReference   = MeshBind * ReferenceMeshBindInverse;
        const FMatrix NormalToReference = MeshToReference.GetInverse().GetTransposed();

        TArray<TArray<FImportedBoneWeight>> ControlPointWeight;
        ExtractSkinWeightsOnly(Mesh, BoneNodeToIndex, ControlPointWeight);

        int32 PolygonVertexIndex = 0;

        for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
        {
            const int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);

            if (PolygonSize != 3)
            {
                PolygonVertexIndex += PolygonSize;
                continue;
            }

            const int32 LocalMaterialIndex = GetPolygonMaterialIndex(Mesh, PolygonIndex);

            FbxSurfaceMaterial* FbxMat = nullptr;

            if (LocalMaterialIndex >= 0 && LocalMaterialIndex < MeshNode->GetMaterialCount())
            {
                FbxMat = MeshNode->GetMaterial(LocalMaterialIndex);
            }

            const int32 MaterialIndex = FindOrAddSkeletalMaterial(FbxMat, OutMaterials);

            FImportedSectionBuild* SectionBuild = FindOrAddImportedSection(SectionBuilds, MaterialIndex);

            int32    ControlPointIndices[3] = {};
            FVector  Positions[3];
            FVector2 UV0[3];

            for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
            {
                ControlPointIndices[CornerIndex] = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);

                const FVector LocalPosition = ReadPosition(Mesh, ControlPointIndices[CornerIndex]);

                Positions[CornerIndex] = TransformPositionByMatrix(LocalPosition, MeshToReference);

                UV0[CornerIndex] = ReadUVByChannel(Mesh, PolygonIndex, CornerIndex, 0);
            }

            const FVector FallbackTangent = ComputeTriangleTangent(Positions[0], Positions[1], Positions[2], UV0[0], UV0[1], UV0[2]);

            for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
            {
                const int32 ControlPointIndex = ControlPointIndices[CornerIndex];

                const int32 CurrentPolygonVertexIndex = PolygonVertexIndex + CornerIndex;

                FSkeletalVertex Vertex;

                Vertex.Pos = Positions[CornerIndex];

                Vertex.Normal = TransformNormalByMatrix(ReadNormal(Mesh, PolygonIndex, CornerIndex), NormalToReference);

                const int32 UVCount = static_cast<int32>((std::min<size_t>)(static_cast<size_t>(MAX_SKELETAL_MESH_UV_CHANNELS), static_cast<size_t>(GetUVSetCount(Mesh))));

                Vertex.NumUVs = static_cast<uint8>(UVCount > 0 ? UVCount : 1);

                for (int32 UVIndex = 0; UVIndex < static_cast<int32>(Vertex.NumUVs); ++UVIndex)
                {
                    Vertex.UV[UVIndex] = ReadUVByChannel(Mesh, PolygonIndex, CornerIndex, UVIndex);
                }

                Vertex.Color = ReadVertexColor(Mesh, ControlPointIndex, CurrentPolygonVertexIndex);

                FVector4 ImportedTangent;
                if (TryReadTangent(Mesh, ControlPointIndex, CurrentPolygonVertexIndex, ImportedTangent))
                {
                    const FVector T = TransformTangentByMatrix(FVector(ImportedTangent.X, ImportedTangent.Y, ImportedTangent.Z), MeshToReference, Vertex.Normal);

                    Vertex.Tangent = FVector4(T.X, T.Y, T.Z, ImportedTangent.W);
                }
                else
                {
                    const FVector T = OrthogonalizeTangentToNormal(FallbackTangent, Vertex.Normal);

                    Vertex.Tangent = FVector4(T.X, T.Y, T.Z, 1.0f);
                }

                if (ControlPointIndex >= 0 && ControlPointIndex < static_cast<int32>(ControlPointWeight.size()))
                {
                    PackTop4Weights(ControlPointWeight[ControlPointIndex], Vertex.BoneIndices, Vertex.BoneWeights);
                }
                else
                {
                    TArray<FImportedBoneWeight> EmptyWeight;
                    PackTop4Weights(EmptyWeight, Vertex.BoneIndices, Vertex.BoneWeights);
                }

                const uint32 NewIndex = static_cast<uint32>(OutLOD.Vertices.size());

                OutLOD.Vertices.push_back(Vertex);
                SectionBuild->Indices.push_back(NewIndex);

                if (OutMorphSources)
                {
                    FImportedMorphSourceVertex Source;
                    Source.Mesh                   = Mesh;
                    Source.ControlPointIndex      = ControlPointIndex;
                    Source.PolygonIndex           = PolygonIndex;
                    Source.CornerIndex            = CornerIndex;
                    Source.PolygonVertexIndex     = CurrentPolygonVertexIndex;
                    Source.VertexIndex            = NewIndex;
                    Source.MeshToReference        = MeshToReference;
                    Source.NormalToReference      = NormalToReference;
                    Source.BaseNormalInReference  = Vertex.Normal;
                    Source.BaseTangentInReference = Vertex.Tangent;

                    OutMorphSources->push_back(Source);
                }
            }

            PolygonVertexIndex += PolygonSize;
        }
    }

    if (OutLOD.Vertices.empty() || SectionBuilds.empty())
    {
        return false;
    }

    BuildFinalSkeletalSections(SectionBuilds, OutMaterials, OutLOD.Indices, OutLOD.Sections);

    if (OutLOD.Indices.empty() || OutLOD.Sections.empty())
    {
        return false;
    }

    OutLOD.CacheBounds();

    return true;
}

bool FFbxImporter::ImportStaticMesh(const FString& SourcePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    OutMesh = FStaticMesh();
    OutMaterials.clear();

    FFbxSceneHandle SceneHandle;
    if (!LoadFbxScene(SourcePath, SceneHandle))
    {
        return false;
    }

    TriangulateScene(SceneHandle.Manager, SceneHandle.Scene);

    TArray<FbxNode*> MeshNodes;
    CollectMeshNodes(SceneHandle.Scene->GetRootNode(), MeshNodes);

    if (MeshNodes.empty())
    {
        return false;
    }

    OutMesh.PathFileName = SourcePath;

    GetOrCreateDefaultMaterial(OutMaterials);

    FStaticMeshSection Section;
    Section.MaterialIndex    = 0;
    Section.MaterialSlotName = OutMaterials[0].MaterialSlotName;
    Section.FirstIndex       = 0;
    Section.NumTriangles     = 0;

    for (FbxNode* MeshNode : MeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (!Mesh)
        {
            continue;
        }

        for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
        {
            const int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);

            if (PolygonSize != 3)
            {
                continue;
            }

            for (int32 CornerIndex = 0; CornerIndex < PolygonSize; ++CornerIndex)
            {
                const int32 ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);

                FNormalVertex Vertex;
                Vertex.pos     = ReadPosition(Mesh, ControlPointIndex);
                Vertex.normal  = ReadNormal(Mesh, PolygonIndex, CornerIndex);
                Vertex.tex     = ReadUV(Mesh, PolygonIndex, CornerIndex);
                Vertex.color   = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
                Vertex.tangent = FVector4(1.0f, 0.0f, 0.0f, 1.0f);

                const uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.size());

                OutMesh.Vertices.push_back(Vertex);
                OutMesh.Indices.push_back(NewIndex);
            }

            Section.NumTriangles++;
        }
    }

    if (OutMesh.Vertices.empty() || OutMesh.Indices.empty())
    {
        return false;
    }

    OutMesh.Sections.push_back(Section);
    OutMesh.CacheBounds();

    return true;
}

bool FFbxImporter::ImportSkeletalMesh(const FString& SourcePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    OutMesh = FSkeletalMesh();
    OutMaterials.clear();

    FFbxSceneHandle SceneHandle;

    if (!LoadFbxScene(SourcePath, SceneHandle))
    {
        return false;
    }

    NormalizeFbxScene(SceneHandle.Scene);

    TriangulateScene(SceneHandle.Manager, SceneHandle.Scene);

    OutMesh.PathFileName = SourcePath;

    TArray<FbxNode*> MeshNodes;
    CollectMeshNodes(SceneHandle.Scene->GetRootNode(), MeshNodes);

    TArray<FbxNode*> SkinnedMeshNodes;

    for (FbxNode* MeshNode : MeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (Mesh && MeshHasSkin(Mesh))
        {
            SkinnedMeshNodes.push_back(MeshNode);
        }
    }

    if (SkinnedMeshNodes.empty())
    {
        return false;
    }

    TMap<FbxNode*, int32> BoneNodeToIndex;
    if (!BuildSkeletonFromSkinClusters(SkinnedMeshNodes, OutMesh.Skeleton, BoneNodeToIndex))
    {
        return false;
    }

    TMap<int32, TArray<FbxNode*>> MeshNodesByLOD;

    for (FbxNode* MeshNode : SkinnedMeshNodes)
    {
        const int32 LODIndex = GetSkeletalMeshLODIndex(MeshNode);
        MeshNodesByLOD[LODIndex].push_back(MeshNode);
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

    const int32             ReferenceLODIndex = SortedLODIndices[0];
    const TArray<FbxNode*>& ReferenceLODNodes = MeshNodesByLOD[ReferenceLODIndex];

    FbxAMatrix ReferenceMeshBindFbx;
    if (!TryGetReferenceMeshBindMatrix(ReferenceLODNodes, ReferenceMeshBindFbx))
    {
        return false;
    }

    const FMatrix ReferenceMeshBind        = ToEngineMatrix(ReferenceMeshBindFbx);
    const FMatrix ReferenceMeshBindInverse = ReferenceMeshBind.GetInverse();

    InitializeBoneBindPoseFromSceneNodes(BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton);

    TArray<bool> AppliedClusterBindPose;
    AppliedClusterBindPose.resize(OutMesh.Skeleton.Bones.size());

    ApplyBindPoseFromSkinClusters(ReferenceLODNodes, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, &AppliedClusterBindPose);
    ApplyBindPoseFromSkinClusters(SkinnedMeshNodes, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, &AppliedClusterBindPose);

    RecomputeLocalBindPose(OutMesh.Skeleton);

    TArray<TArray<FImportedMorphSourceVertex>> MorphSourcesByLOD;

    for (int32 LODIndex : SortedLODIndices)
    {
        FSkeletalMeshLOD                   NewLOD;
        TArray<FImportedMorphSourceVertex> MorphSources;

        if (!BuildSkeletalMeshLODFromNodes(MeshNodesByLOD[LODIndex], BoneNodeToIndex, ReferenceMeshBindInverse, OutMaterials, NewLOD, &MorphSources))
        {
            continue;
        }

        OutMesh.LODModels.push_back(NewLOD);
        MorphSourcesByLOD.push_back(std::move(MorphSources));
    }

    if (OutMesh.LODModels.empty())
    {
        return false;
    }

    ImportMorphTargets(MorphSourcesByLOD, OutMesh.MorphTargets);

    ImportAnimations(SceneHandle.Scene, BoneNodeToIndex, ReferenceMeshBindInverse, OutMesh.Skeleton, OutMesh.Animations);

    return true;
}

bool FFbxImporter::HasSkinDeformer(const FString& SourcePath)
{
    FFbxSceneHandle SceneHandle;
    if (!LoadFbxScene(SourcePath, SceneHandle))
    {
        return false;
    }

    TArray<FbxNode*> MeshNodes;
    CollectMeshNodes(SceneHandle.Scene->GetRootNode(), MeshNodes);

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
