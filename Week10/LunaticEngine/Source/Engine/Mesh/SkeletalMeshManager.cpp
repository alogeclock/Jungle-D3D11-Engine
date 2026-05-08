#include "SkeletalMeshManager.h"

#include <filesystem>
#include <algorithm>

#include "SkeletalMeshBake.h"
#include "Platform/Paths.h"
#include "Object/Object.h"
#include "Materials/MaterialManager.h"

TMap<FString, USkeletalMesh*> FSkeletalMeshManager::SkeletalMeshCache;

namespace
{
    std::filesystem::path GetMeshCacheDirectory(const FString& OriginalPath)
    {
        std::filesystem::path SourcePath(FPaths::ToWide(OriginalPath));
        return SourcePath.parent_path() / L"Cache";
    }

    void EnsureParentDirExistsForFile(const FString& FilePath)
    {
        std::filesystem::path Path(FPaths::ToWide(FilePath));
        FPaths::CreateDir(Path.parent_path().wstring());
    }
}

FString FSkeletalMeshManager::GetBakedFilePath(const FString& SourcePath)
{
    std::filesystem::path SrcPath(FPaths::ToWide(SourcePath));
    std::wstring          Ext = SrcPath.extension().wstring();

    std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

    if (Ext == L".skm")
    {
        return SourcePath;
    }

    std::filesystem::path BakePath = GetMeshCacheDirectory(SourcePath) / SrcPath.stem();
    BakePath                       += L".skm";

    return FPaths::ToUtf8(BakePath.lexically_normal().generic_wstring());
}

USkeletalMesh* FSkeletalMeshManager::LoadSkeletalMesh(const FString& PathFileName)
{
    const FString CacheKey = GetBakedFilePath(PathFileName);

    auto It = SkeletalMeshCache.find(CacheKey);
    if (It != SkeletalMeshCache.end())
    {
        return It->second;
    }

    FSkeletalMesh*          NewMeshAsset = new FSkeletalMesh();
    TArray<FStaticMaterial> Materials;

    const bool bLoaded = LoadBakedSkeletalMesh(CacheKey, *NewMeshAsset, Materials);

    if (!bLoaded)
    {
        delete NewMeshAsset;

        // TODO : FBX Importer fallback
        return nullptr;
    }

    USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();

    SkeletalMesh->SetSkeletalMaterials(std::move(Materials));
    SkeletalMesh->SetSkeletalMeshAsset(NewMeshAsset);

    SkeletalMeshCache[CacheKey] = SkeletalMesh;

    FMaterialManager::Get().ScanMaterialAssets();

    return SkeletalMesh;
}

bool FSkeletalMeshManager::LoadBakedSkeletalMesh(const FString& BakedPath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
    return SkeletalMeshBake::Load(BakedPath, OutMesh, OutMaterials);
}

bool FSkeletalMeshManager::SaveBakedSkeletalMesh(const FString& BakedPath, FSkeletalMesh& Mesh, TArray<FStaticMaterial>& Materials)
{
    EnsureParentDirExistsForFile(BakedPath);
    return SkeletalMeshBake::Save(BakedPath, Mesh, Materials);
}
