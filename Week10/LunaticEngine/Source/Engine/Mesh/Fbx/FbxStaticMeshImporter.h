#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>

struct FStaticMesh;
struct FStaticMaterial;
struct FFbxImportContext;

class FFbxStaticMeshImporter
{
public:
    // FBX scene의 mesh node들을 하나의 static mesh asset으로 import한다.
    static bool Import(
        FbxScene*                Scene,
        const FString&           SourcePath,
        FStaticMesh&             OutMesh,
        TArray<FStaticMaterial>& OutMaterials,
        FFbxImportContext&       BuildContext
        );

    static bool ImportMeshNodes(
        const TArray<FbxNode*>&  MeshNodes,
        const FString&           SourcePath,
        FStaticMesh&             OutMesh,
        TArray<FStaticMaterial>& OutMaterials,
        FFbxImportContext&       BuildContext
        );
};
