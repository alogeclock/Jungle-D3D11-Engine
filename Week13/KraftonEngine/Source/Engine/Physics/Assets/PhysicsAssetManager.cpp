#include "PhysicsAssetManager.h"

#include "Asset/AssetPackage.h"
#include "Object/FUObjectArray.h"
#include "PhysicsAsset.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <utility>

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

void FPhysicsAssetManager::ScanPhysicsAssets()
{
    AvailablePhysicsAssetFiles.clear();

    const std::filesystem::path ProjectRoot(FPaths::RootDir());
    const std::filesystem::path AssetRoots[] = {
        ProjectRoot / L"Asset",
        ProjectRoot / L"Content",
        ProjectRoot / L"Data",
    };

    for (const std::filesystem::path& AssetRoot : AssetRoots)
    {
        if (!std::filesystem::exists(AssetRoot))
        {
            continue;
        }

        for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
        {
            if (!Entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path& Path = Entry.path();
            std::wstring Extension = Path.extension().wstring();
            std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
            if (Extension != L".uasset")
            {
                continue;
            }

            const FString RelPath = FPaths::MakeProjectRelative(
                FPaths::ToUtf8(Path.lexically_normal().generic_wstring()));

            EAssetPackageType PackageType = EAssetPackageType::Unknown;
            if (!FAssetPackage::GetPackageType(RelPath, PackageType)
                || PackageType != EAssetPackageType::PhysicsAsset)
            {
                continue;
            }

            FPhysicsAssetListItem Item;
            Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
            Item.FullPath = RelPath;
            AvailablePhysicsAssetFiles.push_back(std::move(Item));
        }
    }

    std::sort(AvailablePhysicsAssetFiles.begin(), AvailablePhysicsAssetFiles.end(),
        [](const FPhysicsAssetListItem& A, const FPhysicsAssetListItem& B)
        {
            return A.FullPath < B.FullPath;
        });
}
