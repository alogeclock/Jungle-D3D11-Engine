#include "Mesh/FbxImporter.h"

#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxSceneLoader.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxSkeletalMeshImporter.h"
#include "Mesh/Fbx/FbxStaticMeshImporter.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/StaticMeshAsset.h"

// FBX 파일에서 static mesh geometry, transform, vertex attributes, material section 정보를 import한다.
bool FFbxImporter::ImportStaticMesh(const FString& SourcePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    OutMesh = FStaticMesh();
    OutMaterials.clear();

    FFbxSceneHandle SceneHandle;
    if (!FFbxSceneLoader::Load(SourcePath, SceneHandle))
    {
        return false;
    }

    FFbxSceneLoader::Normalize(SceneHandle.Scene);
    FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);

    FFbxImportContext BuildContext;
    BuildContext.Summary.SourcePath = SourcePath;

    if (!FFbxStaticMeshImporter::Import(SceneHandle.Scene, SourcePath, OutMesh, OutMaterials, BuildContext))
    {
        OutMesh = FStaticMesh();
        OutMaterials.clear();
        return false;
    }

    OutMesh.PathFileName = SourcePath;
    return true;
}

// FBX 파일에서 skeletal mesh, skeleton, LOD, animation, morph target을 import한다.
bool FFbxImporter::ImportSkeletalMesh(const FString& SourcePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    OutMesh = FSkeletalMesh();
    OutMaterials.clear();

    FFbxSceneHandle SceneHandle;
    if (!FFbxSceneLoader::Load(SourcePath, SceneHandle))
    {
        return false;
    }

    FFbxSceneLoader::Normalize(SceneHandle.Scene);
    FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);

    FFbxImportContext BuildContext;
    BuildContext.Summary.SourcePath = SourcePath;

    if (!FFbxSkeletalMeshImporter::Import(SceneHandle.Scene, SourcePath, OutMesh, OutMaterials, BuildContext))
    {
        OutMesh = FSkeletalMesh();
        OutMaterials.clear();
        return false;
    }

    OutMesh.PathFileName = SourcePath;
    return true;
}

// FBX 파일에 skin deformer가 포함된 mesh가 있는지 검사한다.
bool FFbxImporter::HasSkinDeformer(const FString& SourcePath)
{
    FFbxSceneHandle SceneHandle;
    if (!FFbxSceneLoader::Load(SourcePath, SceneHandle))
    {
        return false;
    }

    return FFbxSceneQuery::SceneHasSkinDeformer(SceneHandle.Scene);
}
