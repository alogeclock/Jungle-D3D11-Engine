#include "Physics/Common/PhysicalMaterialManager.h"

#include "Asset/AssetPackage.h"
#include "Object/FUObjectArray.h"
#include "Physics/Common/PhysicsMaterialTypes.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

UPhysicalMaterial* FPhysicalMaterialManager::Load(const FString& Path)
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
	if (!Header.IsValid(EAssetPackageType::PhysicalMaterial))
	{
		return nullptr;
	}

	FAssetImportMetadata Metadata;
	Ar << Metadata;

	UPhysicalMaterial* Asset = GUObjectArray.CreateObject<UPhysicalMaterial>();
	Asset->Serialize(Ar);
	if (!Ar.IsValid())
	{
		GUObjectArray.DestroyObject(Asset);
		return nullptr;
	}

	Asset->SetAssetPathFileName(NormalizedPath);
	LoadedAssets.emplace(NormalizedPath, Asset);
	return Asset;
}

UPhysicalMaterial* FPhysicalMaterialManager::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	auto It = LoadedAssets.find(NormalizedPath);
	return It != LoadedAssets.end() ? It->second : nullptr;
}

bool FPhysicalMaterialManager::Unload(const FString& Path)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	auto It = LoadedAssets.find(NormalizedPath);
	if (It == LoadedAssets.end())
	{
		return false;
	}

	if (It->second)
	{
		GUObjectArray.DestroyObject(It->second);
	}

	LoadedAssets.erase(It);
	return true;
}

bool FPhysicalMaterialManager::Save(UPhysicalMaterial* Asset)
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
	Header.Type = static_cast<uint32>(EAssetPackageType::PhysicalMaterial);

	FAssetImportMetadata Metadata;

	Ar << Header;
	Ar << Metadata;
	Asset->Serialize(Ar);
	return Ar.IsValid();
}

