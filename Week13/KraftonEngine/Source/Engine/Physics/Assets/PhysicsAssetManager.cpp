#include "PhysicsAssetManager.h"

#include "Asset/AssetPackage.h"
#include "Object/FUObjectArray.h"
#include "PhysicsAsset.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

UPhysicsAsset* FPhysicsAssetManager::Load(const FString& Path)
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

    auto It = LoadedAssets.find(NormalizedPath);
    if (It != LoadedAssets.end())
    {
        return It->second;
    }

    if (!FAssetPackage::IsAssetPackagePath(NormalizedPath))
    {
        return nullptr;
    }

    FWindowsBinReader Ar(NormalizedPath);
    if (!Ar.IsValid())
    {
        return nullptr;
    }

    FAssetPackageHeader Header;
    Ar << Header;
    if (!Header.IsValid(EAssetPackageType::PhysicsAsset))
    {
        return nullptr;
    }

    FAssetImportMetadata Metadata;
    Ar << Metadata;

    UPhysicsAsset* NewAsset = GUObjectArray.CreateObject<UPhysicsAsset>();
    NewAsset->Serialize(Ar);

    if (!Ar.IsValid())
    {
        GUObjectArray.DestroyObject(NewAsset);
        return nullptr;
    }

    NewAsset->SetAssetPathFileName(NormalizedPath);
    LoadedAssets.emplace(NormalizedPath, NewAsset);
    return NewAsset;
}

UPhysicsAsset* FPhysicsAssetManager::Find(const FString& Path) const
{
    const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
    auto It = LoadedAssets.find(NormalizedPath);
    return It != LoadedAssets.end() ? It->second : nullptr;
}

bool FPhysicsAssetManager::Save(UPhysicsAsset* Asset)
{
    if (!Asset)
    {
        return false;
    }

    const FString& Path = Asset->GetAssetPathFileName();
    if (Path.empty() || !FAssetPackage::IsAssetPackagePath(Path))
    {
        return false;
    }

    FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
    if (!Ar.IsValid())
    {
        return false;
    }

    FAssetPackageHeader Header;
    Header.Type = static_cast<uint32>(EAssetPackageType::PhysicsAsset);

    FAssetImportMetadata Metadata;

    Ar << Header;
    Ar << Metadata;
    Asset->Serialize(Ar);

    return Ar.IsValid();
}
