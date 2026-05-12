#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>
#include "Math/Vector.h"
#include "Mesh/StaticMeshAsset.h"

struct FFbxImportContext;

struct FFbxImportedMaterialInfo
{
    FString SlotName;

    FString DiffuseTexture;
    FString NormalTexture;
    FString RoughnessTexture;
    FString MetallicTexture;
    FString EmissiveTexture;
    FString OpacityTexture;

    FVector4 BaseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
};

class FFbxMaterialImporter
{
public:
    // FBX material을 engine material slot으로 변환하거나 기존 slot index를 반환한다.
    static int32 FindOrAddMaterial(
        FbxSurfaceMaterial*      FbxMaterial,
        const FString&           SourceFbxPath,
        TArray<FStaticMaterial>& OutMaterials,
        FFbxImportContext&       Context
        );

    // material이 없는 polygon을 위해 None material slot을 보장한다.
    static void GetOrCreateDefaultMaterial(TArray<FStaticMaterial>& OutMaterials);
};
