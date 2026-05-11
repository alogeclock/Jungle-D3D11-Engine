#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>

struct FSkeletalMesh;
struct FStaticMaterial;
struct FFbxImportContext;

class FFbxSkeletalMeshImporter
{
public:
    // FBX scene에서 skeletal mesh, skeleton, LOD, morph target, animation을 import한다.
    static bool Import(
        FbxScene*                Scene,
        const FString&           SourcePath,
        FSkeletalMesh&           OutMesh,
        TArray<FStaticMaterial>& OutMaterials,
        FFbxImportContext&       BuildContext
        );
};
