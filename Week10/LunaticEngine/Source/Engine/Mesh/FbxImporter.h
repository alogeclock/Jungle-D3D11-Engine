#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/StaticMeshAsset.h"

struct FStaticMaterial;

struct FFbxImportedStaticMeshResult
{
    FString                 SourceNodeName;
    FString                 SuggestedAssetPath;
    FStaticMesh             Mesh;
    TArray<FStaticMaterial> Materials;
};

struct FFbxImportResult
{
    bool                    bHasSkeletalMesh = false;
    FSkeletalMesh           SkeletalMesh;
    TArray<FStaticMaterial> SkeletalMaterials;

    TArray<FFbxImportedStaticMeshResult> StaticMeshes;
    TArray<FString>                      GeneratedAssetPaths;
};

struct FFbxImportOptions
{
    bool bImportSkeletalMesh       = true;
    bool bImportStaticMeshIfNoSkin = true;
    bool bSplitLooseStaticMeshes   = true;
};

class FFbxImporter
{
public:
    static bool ImportStaticMesh(const FString& SourcePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials, FString* OutMessage = nullptr);

    static bool ImportSkeletalMesh(const FString& SourcePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials, FString* OutMessage = nullptr);

    static bool ImportScene(const FString& SourcePath, const FFbxImportOptions& Options, FFbxImportResult& OutResult, FString* OutMessage = nullptr);

    static bool HasSkinDeformer(const FString& SourcePath, FString* OutMessage = nullptr);
};
