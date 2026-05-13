#include "Mesh/SkeletalMeshManager.h"

#include <filesystem>
#include <algorithm>
#include <cwctype>

#include "Core/Log.h"
#include "Mesh/SkeletalMeshBake.h"
#include "Mesh/FbxImporter.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Runtime/Engine.h"
#include "Object/Object.h"
#include "Materials/MaterialManager.h"
#include "Mesh/ObjManager.h"

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
        if (GEngine && !It->second->GetMeshBuffer())
        {
            It->second->InitResources(GEngine->GetRenderer().GetFD3DDevice().GetDevice());
        }
        return It->second;
    }

    FSkeletalMesh*          NewMeshAsset = new FSkeletalMesh();
    TArray<FStaticMaterial> Materials;

    bool bNeedRebuild = true;

    const std::filesystem::path RequestedPathW(FPaths::ToWide(PathFileName));
    std::wstring                RequestedExt = RequestedPathW.extension().wstring();
    std::transform(RequestedExt.begin(), RequestedExt.end(), RequestedExt.begin(), ::towlower);

    const std::filesystem::path BakedPathW(FPaths::ToWide(CacheKey));

    if (RequestedExt == L".skm")
    {
        if (LoadBakedSkeletalMesh(CacheKey, *NewMeshAsset, Materials))
        {
            bNeedRebuild = false;
        }
        else
        {
            delete NewMeshAsset;
            return nullptr;
        }
    }
    else if (std::filesystem::exists(BakedPathW))
    {
        const bool bSourceMissing = !std::filesystem::exists(RequestedPathW);
        const bool bBakedIsFresh  = bSourceMissing || std::filesystem::last_write_time(BakedPathW) >= std::filesystem::last_write_time(RequestedPathW);

        if (bBakedIsFresh)
        {
            if (LoadBakedSkeletalMesh(CacheKey, *NewMeshAsset, Materials))
            {
                bNeedRebuild = false;
            }
            else
            {
                bNeedRebuild = true;
            }
        }
    }

    if (bNeedRebuild)
    {
        if (!FFbxImporter::ImportSkeletalMesh(PathFileName, *NewMeshAsset, Materials))
        {
            delete NewMeshAsset;
            return nullptr;
        }

        NewMeshAsset->PathFileName = PathFileName;
        const bool bSaved          = SaveBakedSkeletalMesh(CacheKey, *NewMeshAsset, Materials);
        if (!bSaved)
        {
            UE_LOG("Failed to save skeletal mesh bake: %s", CacheKey.c_str());
        }
    }
    
    USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();

    SkeletalMesh->SetSkeletalMaterials(std::move(Materials));
    SkeletalMesh->SetSkeletalMeshAsset(NewMeshAsset);
    if (GEngine)
    {
        SkeletalMesh->InitResources(GEngine->GetRenderer().GetFD3DDevice().GetDevice());
    }
    
    SkeletalMeshCache[CacheKey] = SkeletalMesh;

    FObjManager::ScanMeshAssets();
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
