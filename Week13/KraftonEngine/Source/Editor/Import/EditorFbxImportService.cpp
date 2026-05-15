#include "Editor/Import/EditorFbxImportService.h"

#include "Platform/Paths.h"

#if WITH_EDITOR
#include "Asset/AssetPackage.h"
#include "Core/Log.h"
#include "Editor/Import/EditorFbxImporter.h"
#include "Materials/MaterialManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Serialization/WindowsArchive.h"
#endif

#include <algorithm>
#include <cwctype>
#if WITH_EDITOR
#include <exception>
#endif
#include <filesystem>
#if WITH_EDITOR
#include <memory>
#endif

TArray<FMeshAssetListItem> FEditorFbxImportService::AvailableFbxFiles;

namespace
{
#if WITH_EDITOR
	std::wstring GetLowerExtension(const FString& Path)
	{
		std::filesystem::path SrcPath(FPaths::ToWide(Path));
		std::wstring Ext = SrcPath.extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		return Ext;
	}

	FString NormalizeProjectPath(const FString& Path)
	{
		return FPaths::MakeProjectRelative(Path);
	}

	std::filesystem::path ResolveProjectPath(const FString& Path)
	{
		std::filesystem::path FullPath(FPaths::ToWide(Path));
		if (!FullPath.is_absolute())
		{
			FullPath = std::filesystem::path(FPaths::RootDir()) / FullPath;
		}
		return FullPath.lexically_normal();
	}

	bool TryGetSourceFileState(const FString& SourcePath, uint64& OutTimestamp, uint64& OutFileSize)
	{
		const std::filesystem::path FullPath = ResolveProjectPath(SourcePath);
		if (!std::filesystem::exists(FullPath) || !std::filesystem::is_regular_file(FullPath))
		{
			return false;
		}

		OutFileSize = static_cast<uint64>(std::filesystem::file_size(FullPath));
		const auto WriteTime = std::filesystem::last_write_time(FullPath);
		OutTimestamp = static_cast<uint64>(WriteTime.time_since_epoch().count());
		return true;
	}

	FAssetImportMetadata MakeImportMetadata(const FString& SourcePath)
	{
		FAssetImportMetadata Metadata;
		Metadata.SourcePath = NormalizeProjectPath(SourcePath);
		TryGetSourceFileState(SourcePath, Metadata.SourceTimestamp, Metadata.SourceFileSize);
		return Metadata;
	}

	FString BuildAdjacentPackagePath(const FString& FbxFilePath, const wchar_t* Suffix)
	{
		const std::filesystem::path FbxPath = ResolveProjectPath(FbxFilePath);
		std::filesystem::path PackagePath = FbxPath;
		PackagePath.replace_filename(FbxPath.stem().wstring() + Suffix + L".uasset");
		return NormalizeProjectPath(FPaths::ToUtf8(PackagePath.generic_wstring()));
	}

	bool SaveStaticMeshPackage(UStaticMesh* StaticMesh, const FString& PackagePath, const FString& SourcePath)
	{
		std::filesystem::create_directories(ResolveProjectPath(PackagePath).parent_path());

		FWindowsBinWriter Writer(PackagePath);
		if (!Writer.IsValid())
		{
			UE_LOG("FBX static import package save failed: could not open file. Path=%s", PackagePath.c_str());
			return false;
		}

		try
		{
			FAssetPackageHeader Header;
			Header.Type = static_cast<uint32>(EAssetPackageType::StaticMesh);
			Writer << Header;

			FAssetImportMetadata Metadata = MakeImportMetadata(SourcePath);
			Writer << Metadata;

			StaticMesh->Serialize(Writer);
		}
		catch (const std::exception&)
		{
			UE_LOG("FBX static import package save failed: serialization threw an exception. Path=%s", PackagePath.c_str());
			return false;
		}

		return Writer.IsValid();
	}

	bool SaveSkeletalMeshPackage(USkeletalMesh* SkeletalMesh, const FString& PackagePath, const FString& SourcePath)
	{
		std::filesystem::create_directories(ResolveProjectPath(PackagePath).parent_path());

		FWindowsBinWriter Writer(PackagePath);
		if (!Writer.IsValid())
		{
			UE_LOG("FBX skeletal import package save failed: could not open file. Path=%s", PackagePath.c_str());
			return false;
		}

		try
		{
			FAssetPackageHeader Header;
			Header.Type = static_cast<uint32>(EAssetPackageType::SkeletalMesh);
			Writer << Header;

			FAssetImportMetadata Metadata = MakeImportMetadata(SourcePath);
			Writer << Metadata;

			SkeletalMesh->Serialize(Writer);
		}
		catch (const std::exception&)
		{
			UE_LOG("FBX skeletal import package save failed: serialization threw an exception. Path=%s", PackagePath.c_str());
			return false;
		}

		return Writer.IsValid();
	}

	void RefreshAssetLists()
	{
		FMeshManager::ScanMeshAssets();
		FMaterialManager::Get().ScanMaterialAssets();
		FEditorFbxImportService::ScanFbxSourceFiles();
	}
#else
	FString NormalizeProjectPath(const FString& Path)
	{
		return FPaths::MakeProjectRelative(Path);
	}

	std::filesystem::path ResolveProjectPath(const FString& Path)
	{
		std::filesystem::path FullPath(FPaths::ToWide(Path));
		if (!FullPath.is_absolute())
		{
			FullPath = std::filesystem::path(FPaths::RootDir()) / FullPath;
		}
		return FullPath.lexically_normal();
	}

	FString BuildAdjacentPackagePath(const FString& FbxFilePath, const wchar_t* Suffix)
	{
		const std::filesystem::path FbxPath = ResolveProjectPath(FbxFilePath);
		std::filesystem::path PackagePath = FbxPath;
		PackagePath.replace_filename(FbxPath.stem().wstring() + Suffix + L".uasset");
		return NormalizeProjectPath(FPaths::ToUtf8(PackagePath.generic_wstring()));
	}
#endif
}

FString FEditorFbxImportService::GetStaticMeshPackagePathForFbx(const FString& FbxFilePath)
{
	return BuildAdjacentPackagePath(FbxFilePath, L"_StaticMesh");
}

FString FEditorFbxImportService::GetSkeletalMeshPackagePathForFbx(const FString& FbxFilePath)
{
	return BuildAdjacentPackagePath(FbxFilePath, L"_SkeletalMesh");
}

void FEditorFbxImportService::ScanFbxSourceFiles()
{
	AvailableFbxFiles.clear();

	const std::filesystem::path AssetRoot(FPaths::AssetDir());
	if (!std::filesystem::exists(AssetRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".fbx") continue;

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableFbxFiles.push_back(std::move(Item));
	}
}

bool FEditorFbxImportService::ImportStaticMeshFromFbx(const FString& FbxFilePath, ID3D11Device* Device, UStaticMesh*& OutStaticMesh, bool bRefreshAssetLists)
{
	return ImportStaticMeshFromFbx(FbxFilePath, FImportOptions::Default(), Device, OutStaticMesh, bRefreshAssetLists);
}

bool FEditorFbxImportService::ImportStaticMeshFromFbx(const FString& FbxFilePath, const FImportOptions& Options, ID3D11Device* Device, UStaticMesh*& OutStaticMesh, bool bRefreshAssetLists)
{
#if WITH_EDITOR
	OutStaticMesh = nullptr;

	if (!Device)
	{
		UE_LOG("FBX static import failed: D3D device is null. Path=%s", FbxFilePath.c_str());
		return false;
	}

	if (GetLowerExtension(FbxFilePath) != L".fbx")
	{
		UE_LOG("FBX static import failed: unsupported source extension. Path=%s", FbxFilePath.c_str());
		return false;
	}

	std::unique_ptr<FStaticMesh> NewMeshAsset = std::make_unique<FStaticMesh>();
	TArray<FStaticMaterial> ParsedMaterials;
	if (!FEditorFbxImporter::ImportStatic(FbxFilePath, &Options, *NewMeshAsset, ParsedMaterials))
	{
		UE_LOG("FBX static import failed: importer returned empty mesh. Path=%s", FbxFilePath.c_str());
		return false;
	}

	const FString SourcePath = NormalizeProjectPath(FbxFilePath);
	const FString PackagePath = GetStaticMeshPackagePathForFbx(FbxFilePath);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	NewMeshAsset->PathFileName = SourcePath;
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset.release());

	FMeshManager::StaticMeshCache.erase(PackagePath);
	if (!SaveStaticMeshPackage(StaticMesh, PackagePath, FbxFilePath))
	{
		return false;
	}

	StaticMesh->InitResources(Device);
	StaticMesh->SetAssetPathFileName(PackagePath);
	FMeshManager::StaticMeshCache[PackagePath] = StaticMesh;
	OutStaticMesh = StaticMesh;

	if (bRefreshAssetLists)
	{
		RefreshAssetLists();
	}

	UE_LOG("FBX static mesh imported successfully. Source=%s Package=%s", FbxFilePath.c_str(), PackagePath.c_str());
	return true;
#else
	(void)FbxFilePath;
	(void)Options;
	(void)Device;
	(void)bRefreshAssetLists;
	OutStaticMesh = nullptr;
	return false;
#endif
}

bool FEditorFbxImportService::ImportSkeletalMeshFromFbx(const FString& FbxFilePath, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh, bool bRefreshAssetLists)
{
#if WITH_EDITOR
	OutSkeletalMesh = nullptr;

	if (!Device)
	{
		UE_LOG("FBX skeletal import failed: D3D device is null. Path=%s", FbxFilePath.c_str());
		return false;
	}

	if (GetLowerExtension(FbxFilePath) != L".fbx")
	{
		UE_LOG("FBX skeletal import failed: unsupported source extension. Path=%s", FbxFilePath.c_str());
		return false;
	}

	if (!FEditorFbxImporter::Import(FbxFilePath))
	{
		UE_LOG("FBX skeletal import failed: importer returned empty mesh. Path=%s", FbxFilePath.c_str());
		return false;
	}

	std::unique_ptr<FSkeletalMesh> NewMeshAsset = std::make_unique<FSkeletalMesh>();
	NewMeshAsset->PathFileName = NormalizeProjectPath(FbxFilePath);
	NewMeshAsset->Vertices = std::move(FEditorFbxImporter::Vertices);
	NewMeshAsset->Indices = std::move(FEditorFbxImporter::Indices);
	NewMeshAsset->Sections = std::move(FEditorFbxImporter::Sections);
	NewMeshAsset->MeshRanges = std::move(FEditorFbxImporter::MeshRanges);
	NewMeshAsset->Bones = std::move(FEditorFbxImporter::Bones);

	const FString PackagePath = GetSkeletalMeshPackagePathForFbx(FbxFilePath);

	USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
	SkeletalMesh->SetSkeletalMaterials(std::move(FEditorFbxImporter::SkeletalMaterials));
	SkeletalMesh->SetSkeletalMeshAsset(NewMeshAsset.release());

	FMeshManager::SkeletalMeshCache.erase(PackagePath);
	if (!SaveSkeletalMeshPackage(SkeletalMesh, PackagePath, FbxFilePath))
	{
		return false;
	}

	SkeletalMesh->InitResources(Device);
	SkeletalMesh->SetAssetPathFileName(PackagePath);
	FMeshManager::SkeletalMeshCache[PackagePath] = SkeletalMesh;
	OutSkeletalMesh = SkeletalMesh;

	if (bRefreshAssetLists)
	{
		RefreshAssetLists();
	}

	UE_LOG("FBX skeletal mesh imported successfully. Source=%s Package=%s", FbxFilePath.c_str(), PackagePath.c_str());
	return true;
#else
	(void)FbxFilePath;
	(void)Device;
	(void)bRefreshAssetLists;
	OutSkeletalMesh = nullptr;
	return false;
#endif
}
