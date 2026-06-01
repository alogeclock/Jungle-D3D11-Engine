#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class UPhysicsAsset;

struct FPhysicsAssetListItem
{
    FString DisplayName;
    FString FullPath;
};

class FPhysicsAssetManager : public TSingleton<FPhysicsAssetManager>
{
    friend class TSingleton<FPhysicsAssetManager>;

public:
    UPhysicsAsset* Load(const FString& Path);
    UPhysicsAsset* Find(const FString& Path) const;

    bool Save(UPhysicsAsset* Asset);
    bool SaveAsJson(UPhysicsAsset* Asset, const FString& OverridePath = FString());

    void ScanPhysicsAssets();
    const TArray<FPhysicsAssetListItem>& GetAvailablePhysicsAssetFiles() const { return AvailablePhysicsAssetFiles; }

private:
    TMap<FString, UPhysicsAsset*> LoadedAssets;
    TArray<FPhysicsAssetListItem> AvailablePhysicsAssetFiles;
};
