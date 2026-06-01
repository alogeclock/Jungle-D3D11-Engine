#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class UPhysicsAsset;

class FPhysicsAssetManager : public TSingleton<FPhysicsAssetManager>
{
    friend class TSingleton<FPhysicsAssetManager>;

public:
    UPhysicsAsset* Load(const FString& Path);
    UPhysicsAsset* Find(const FString& Path) const;

    bool Save(UPhysicsAsset* Asset);

private:
    TMap<FString, UPhysicsAsset*> LoadedAssets;
};
