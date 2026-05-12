#include "Mesh/FbxImporter.h"

#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxSceneLoader.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxSkeletalMeshImporter.h"
#include "Mesh/Fbx/FbxStaticMeshImporter.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/StaticMeshAsset.h"

#include <filesystem>

// FBX 파일에서 static mesh geometry, transform, vertex attributes, material section 정보를 import한다.
bool FFbxImporter::ImportStaticMesh(const FString& SourcePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials, FString* OutMessage)
{
    OutMesh = FStaticMesh();
    OutMaterials.clear();

    FFbxSceneHandle SceneHandle;
    if (!FFbxSceneLoader::Load(SourcePath, SceneHandle, OutMessage))
    {
        return false;
    }

    FFbxSceneLoader::Normalize(SceneHandle.Scene);
    FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);

    FFbxImportContext BuildContext;
    BuildContext.Summary.SourcePath = SourcePath;

    if (!FFbxStaticMeshImporter::Import(SceneHandle.Scene, SourcePath, OutMesh, OutMaterials, BuildContext))
    {
        if (OutMessage)
        {
            *OutMessage = "Failed to import FBX as static mesh: " + SourcePath;
        }
        OutMesh = FStaticMesh();
        OutMaterials.clear();
        return false;
    }

    OutMesh.PathFileName = SourcePath;
    return true;
}

// FBX 파일에서 skeletal mesh, skeleton, LOD, animation, morph target을 import한다.
bool FFbxImporter::ImportSkeletalMesh(const FString& SourcePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials, FString* OutMessage)
{
    OutMesh = FSkeletalMesh();
    OutMaterials.clear();

    FFbxSceneHandle SceneHandle;
    if (!FFbxSceneLoader::Load(SourcePath, SceneHandle, OutMessage))
    {
        return false;
    }

    FFbxSceneLoader::Normalize(SceneHandle.Scene);
    FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);

    FFbxImportContext BuildContext;
    BuildContext.Summary.SourcePath = SourcePath;

    if (!FFbxSkeletalMeshImporter::Import(SceneHandle.Scene, SourcePath, OutMesh, OutMaterials, BuildContext))
    {
        if (OutMessage)
        {
            *OutMessage = "Failed to import FBX as skeletal mesh: " + SourcePath;
        }
        OutMesh = FSkeletalMesh();
        OutMaterials.clear();
        return false;
    }

    OutMesh.PathFileName = SourcePath;
    return true;
}

bool FFbxImporter::ImportScene(const FString& SourcePath, const FFbxImportOptions& Options, FFbxImportResult& OutResult, FString* OutMessage)
{
    OutResult = FFbxImportResult();

    FFbxSceneHandle SceneHandle;
    if (!FFbxSceneLoader::Load(SourcePath, SceneHandle, OutMessage))
    {
        return false;
    }

    FFbxSceneLoader::Normalize(SceneHandle.Scene);
    FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);

    const bool bHasSkin     = FFbxSceneQuery::SceneHasSkinDeformer(SceneHandle.Scene);
    bool       bImportedAny = false;

    if (bHasSkin && Options.bImportSkeletalMesh)
    {
        FFbxImportContext BuildContext;
        BuildContext.Summary.SourcePath = SourcePath;
        if (FFbxSkeletalMeshImporter::Import(SceneHandle.Scene, SourcePath, OutResult.SkeletalMesh, OutResult.SkeletalMaterials, BuildContext))
        {
            OutResult.SkeletalMesh.PathFileName = SourcePath;
            OutResult.bHasSkeletalMesh          = true;
            bImportedAny                        = true;

            for (const FFbxSplitStaticMeshReference& SplitRef : OutResult.SkeletalMesh.SplitStaticMeshes)
            {
                if (!SplitRef.StaticMeshAssetPath.empty())
                {
                    OutResult.GeneratedAssetPaths.push_back(SplitRef.StaticMeshAssetPath);
                }
            }
            for (const FSkeletalStaticChildMesh& StaticChild : OutResult.SkeletalMesh.StaticChildMeshes)
            {
                if (!StaticChild.StaticMeshAssetPath.empty())
                {
                    OutResult.GeneratedAssetPaths.push_back(StaticChild.StaticMeshAssetPath);
                }
            }
        }
    }
    else if (!bHasSkin && Options.bImportStaticMeshIfNoSkin)
    {
        FFbxImportContext BuildContext;
        BuildContext.Summary.SourcePath = SourcePath;
        FFbxImportedStaticMeshResult StaticResult;
        StaticResult.SourceNodeName = "Scene";
        if (FFbxStaticMeshImporter::Import(SceneHandle.Scene, SourcePath, StaticResult.Mesh, StaticResult.Materials, BuildContext))
        {
            StaticResult.Mesh.PathFileName = SourcePath;
            OutResult.StaticMeshes.push_back(std::move(StaticResult));
            bImportedAny = true;
        }
    }

    if (Options.bSplitLooseStaticMeshes && bHasSkin && SceneHandle.Scene && SceneHandle.Scene->GetRootNode())
    {
        TArray<FbxNode*> MeshNodes;
        FFbxSceneQuery::CollectMeshNodes(SceneHandle.Scene->GetRootNode(), MeshNodes);

        for (FbxNode* MeshNode : MeshNodes)
        {
            FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
            if (!Mesh || FFbxSceneQuery::MeshHasSkin(Mesh) || FFbxSceneQuery::IsCollisionProxyNode(MeshNode) || FFbxSceneQuery::FindNearestParentSkeletonNode(
                MeshNode
            ))
            {
                continue;
            }

            FFbxImportContext StaticContext;
            StaticContext.Summary.SourcePath = SourcePath;
            TArray<FbxNode*> SingleNode;
            SingleNode.push_back(MeshNode);

            FFbxImportedStaticMeshResult StaticResult;
            StaticResult.SourceNodeName = MeshNode->GetName() ? FString(MeshNode->GetName()) : FString();
            if (FFbxStaticMeshImporter::ImportMeshNodes(SingleNode, SourcePath, StaticResult.Mesh, StaticResult.Materials, StaticContext))
            {
                StaticResult.Mesh.PathFileName = SourcePath;
                OutResult.StaticMeshes.push_back(std::move(StaticResult));
                bImportedAny = true;
            }
        }
    }

    if (!bImportedAny && OutMessage)
    {
        *OutMessage = "Failed to import FBX scene: " + SourcePath;
    }

    return bImportedAny;
}

// FBX 파일에 skin deformer가 포함된 mesh가 있는지 검사한다.
bool FFbxImporter::HasSkinDeformer(const FString& SourcePath, FString* OutMessage)
{
    FFbxSceneHandle SceneHandle;
    if (!FFbxSceneLoader::Load(SourcePath, SceneHandle, OutMessage))
    {
        return false;
    }

    return FFbxSceneQuery::SceneHasSkinDeformer(SceneHandle.Scene);
}
