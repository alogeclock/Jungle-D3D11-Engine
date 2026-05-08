#include "Mesh/FbxImporter.h"

#include "Mesh/StaticMeshAsset.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Materials/MaterialManager.h"
#include "Engine/Platform/Paths.h"

#include <fbxsdk.h>
#include <algorithm>
#include <string>

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

    static bool IsSkeletonNode(FbxNode* Node)
    {
        if (!Node)
        {
            return false;
        }

        FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
        return Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton;
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

    static void FindRootSkeletonNodes(FbxNode* Node, TArray<FbxNode*>& OutRootNodes)
    {
        if (!Node)
        {
            return;
        }

        if (IsSkeletonNode(Node))
        {
            FbxNode* Parent = Node->GetParent();

            if (!IsSkeletonNode(Parent))
            {
                OutRootNodes.push_back(Node);
            }

            return;
        }

        for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
        {
            FindRootSkeletonNodes(Node->GetChild(ChildIndex), OutRootNodes);
        }
    }

    static int32 AddBoneRecursive(FbxNode* BoneNode, int32 ParentIndex, FSkeleton& OutSkeleton, TMap<FbxNode*, int32>& OutBoneNodeToIndex)
    {
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

            if (IsSkeletonNode(Child))
            {
                AddBoneRecursive(Child, BoneIndex, OutSkeleton, OutBoneNodeToIndex);
            }
        }

        return BoneIndex;
    }

    static bool BuildSkeleton(FbxScene* Scene, FSkeleton& OutSkeleton, TMap<FbxNode*, int32>& OutBoneNodeToIndex)
    {
        if (!Scene)
        {
            return false;
        }

        TArray<FbxNode*> RootNodes;
        FindRootSkeletonNodes(Scene->GetRootNode(), RootNodes);

        for (FbxNode* RootNode : RootNodes)
        {
            AddBoneRecursive(RootNode, -1, OutSkeleton, OutBoneNodeToIndex);
        }

        OutSkeleton.RebuildChildren();

        return !OutSkeleton.Bones.empty();
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

    static void ExtractSkinWeights(FbxMesh* Mesh, const TMap<FbxNode*, int32>& BoneNodeToIndex, FSkeleton& Skeleton, TArray<TArray<FImportedBoneWeight>>& OutControlPointWeight)
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
                if (!Cluster)
                {
                    continue;
                }

                FbxNode* BoneNode = Cluster->GetLink();
                if (!BoneNode)
                {
                    continue;
                }

                auto BoneIt = BoneNodeToIndex.find(BoneNode);
                if (BoneIt == BoneNodeToIndex.end())
                {
                    continue;
                }

                const int32 BoneIndex = BoneIt->second;

                if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(Skeleton.Bones.size()))
                {
                    FbxAMatrix LinkBindMatrix;
                    Cluster->GetTransformLinkMatrix(LinkBindMatrix);

                    Skeleton.Bones[BoneIndex].GlobalBindPose  = ToEngineMatrix(LinkBindMatrix);
                    Skeleton.Bones[BoneIndex].InverseBindPose = Skeleton.Bones[BoneIndex].GlobalBindPose.GetInverse();
                }

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

                    if (Weight <= 0.0f)
                    {
                        continue;
                    }

                    FImportedBoneWeight ImportedWeight;
                    ImportedWeight.BoneIndex = BoneIndex;
                    ImportedWeight.Weight    = Weight;

                    OutControlPointWeight[ControlPointIndex].push_back(ImportedWeight);
                }
            }
        }
    }

    static void PackTop4Weights(const TArray<FImportedBoneWeight>& SourceWeight, uint16 OutBoneIndices[4], float OutboneWeights[4])
    {
        for (int32 i = 0; i < 4; ++i)
        {
            OutBoneIndices[i] = 0;
            OutboneWeights[i] = 0.0f;
        }

        if (SourceWeight.empty())
        {
            OutBoneIndices[0] = 0;
            OutboneWeights[0] = 1.0f;
            return;
        }

        TArray<FImportedBoneWeight> SortedWeights = SourceWeight;
        std::sort(SortedWeights.begin(), SortedWeights.end(), [](const FImportedBoneWeight& A, const FImportedBoneWeight& B)
        {
            return A.Weight > B.Weight;
        });

        const int32 Count = static_cast<int32>(std::min<SIZE_T>(4, SortedWeights.size()));

        float Sum = 0.0f;

        for (int32 i = 0; i < Count; ++i)
        {
            OutBoneIndices[i] = static_cast<uint16>(SortedWeights[i].BoneIndex);
            OutboneWeights[i] = SortedWeights[i].Weight;
            Sum               += OutboneWeights[i];
        }

        if (Sum <= 1e-6f)
        {
            OutBoneIndices[0] = 0;
            OutboneWeights[0] = 1.0f;

            for (int32 i = 1; i < 4; ++i)
            {
                OutBoneIndices[i] = 0;
                OutboneWeights[i] = 0.0f;
            }
            return;
        }

        for (int32 i = 0; i < 4; ++i)
        {
            OutboneWeights[i] /= Sum;
        }
    }
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
                const int32 controlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);

                FNormalVertex Vertex;
                Vertex.pos     = ReadPosition(Mesh, controlPointIndex);
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

    TriangulateScene(SceneHandle.Manager, SceneHandle.Scene);

    OutMesh.PathFileName = SourcePath;

    TMap<FbxNode*, int32> BoneNodeToIndex;
    if (!BuildSkeleton(SceneHandle.Scene, OutMesh.Skeleton, BoneNodeToIndex))
    {
        return false;
    }

    GetOrCreateDefaultMaterial(OutMaterials);

    FSkeletalMeshLOD LOD0;

    FStaticMeshSection Section;
    Section.MaterialIndex    = 0;
    Section.MaterialSlotName = OutMaterials[0].MaterialSlotName;
    Section.FirstIndex       = 0;
    Section.NumTriangles     = 0;

    TArray<FbxNode*> MeshNodes;
    CollectMeshNodes(SceneHandle.Scene->GetRootNode(), MeshNodes);

    for (FbxNode* MeshNode : MeshNodes)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;

        if (!Mesh || !MeshHasSkin(Mesh))
        {
            continue;
        }

        TArray<TArray<FImportedBoneWeight>> ControlPointWeight;
        ExtractSkinWeights(Mesh, BoneNodeToIndex, OutMesh.Skeleton, ControlPointWeight);

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

                FSkeletalVertex Vertex;
                Vertex.Pos     = ReadPosition(Mesh, ControlPointIndex);
                Vertex.Normal  = ReadNormal(Mesh, PolygonIndex, CornerIndex);
                Vertex.UV      = ReadUV(Mesh, PolygonIndex, CornerIndex);
                Vertex.Color   = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
                Vertex.Tangent = FVector4(1.0f, 0.0f, 0.0f, 1.0f);

                if (ControlPointIndex >= 0 && ControlPointIndex < static_cast<int32>(ControlPointWeight.size()))
                {
                    PackTop4Weights(ControlPointWeight[ControlPointIndex], Vertex.BoneIndices, Vertex.BoneWeights);
                }
                else
                {
                    TArray<FImportedBoneWeight> EmptyWeight;
                    PackTop4Weights(EmptyWeight, Vertex.BoneIndices, Vertex.BoneWeights);
                }

                const uint32 NewIndex = static_cast<uint32>(LOD0.Vertices.size());

                LOD0.Vertices.push_back(Vertex);
                LOD0.Indices.push_back(NewIndex);
            }

            Section.NumTriangles++;
        }
    }

    if (LOD0.Vertices.empty() || LOD0.Indices.empty())
    {
        return false;
    }

    LOD0.Sections.push_back(Section);
    LOD0.CacheBounds();

    OutMesh.Skeleton.RebuildChildren();
    OutMesh.LODModels.push_back(LOD0);

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
