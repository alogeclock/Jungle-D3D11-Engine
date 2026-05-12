#pragma once

#include "Core/CoreTypes.h"

struct FStaticMesh;
struct FSkeletalMesh;
struct FStaticMaterial;

class FFbxImporter
{
public:
    static bool ImportStaticMesh(const FString& SourcePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials, FString* OutMessage = nullptr);

    static bool ImportSkeletalMesh(const FString& SourcePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials, FString* OutMessage = nullptr);

    static bool HasSkinDeformer(const FString& SourcePath, FString* OutMessage = nullptr);
};