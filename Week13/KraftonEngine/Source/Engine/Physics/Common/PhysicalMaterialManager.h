#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class UPhysicalMaterial;

class FPhysicalMaterialManager : public TSingleton<FPhysicalMaterialManager>
{
	friend class TSingleton<FPhysicalMaterialManager>;

public:
	UPhysicalMaterial* Load(const FString& Path);
	UPhysicalMaterial* Find(const FString& Path) const;
	bool Unload(const FString& Path);
	bool Save(UPhysicalMaterial* Asset);

private:
	TMap<FString, UPhysicalMaterial*> LoadedAssets;
};

