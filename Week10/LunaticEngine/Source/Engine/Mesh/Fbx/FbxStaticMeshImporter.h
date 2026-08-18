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

    // Mesh node의 global transform을 vertex에 굽지 않고, geometry transform만 반영한다.
    // bone attachment/static child asset 생성처럼 별도 attachment matrix가 보존되는 경우에 사용한다.
    static bool ImportMeshNodesLocal(
        const TArray<FbxNode*>&  MeshNodes,
        const FString&           SourcePath,
        FStaticMesh&             OutMesh,
        TArray<FStaticMaterial>& OutMaterials,
        FFbxImportContext&       BuildContext
        );
};
