#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/SkeletalMeshAsset.h"

#include <fbxsdk.h>

struct FFbxImportContext;

class FFbxStaticChildMeshImporter
{
public:
    static bool ImportAttachedStaticMesh(FbxNode* MeshNode, const FString& SourceFbxPath, FString& OutStaticMeshAssetPath, FFbxImportContext& BuildContext);

    static bool ImportLooseStaticMesh(FbxNode* MeshNode, const FString& SourceFbxPath, FString& OutStaticMeshAssetPath, FFbxImportContext& BuildContext);
};
