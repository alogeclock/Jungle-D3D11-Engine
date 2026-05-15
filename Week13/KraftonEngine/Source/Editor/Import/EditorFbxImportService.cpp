#include "Editor/Import/EditorFbxImportService.h"

#include "Platform/Paths.h"

#if WITH_EDITOR
#include "Asset/AssetPackage.h"
#include "Core/Log.h"
#include "Editor/Import/EditorFbxImporter.h"
#include "Materials/MaterialManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/Skeleton.h"
#include "Mesh/SkeletonManager.h"
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

	FString SanitizeFileStem(const FString& Name)
	{
		FString Result = Name.empty() ? "None" : Name;
		for (char& Ch : Result)
		{
			const unsigned char U = static_cast<unsigned char>(Ch);
			if (U < 32 || Ch == '<' || Ch == '>' || Ch == ':' || Ch == '"' ||
				Ch == '/' || Ch == '\\' || Ch == '|' || Ch == '?' || Ch == '*')
			{
				Ch = '_';
			}
		}
		return Result.empty() ? FString("None") : Result;
	}

	FString BuildSkeletalMeshPackagePath(const FString& FbxFilePath, const FString& MeshNodeName, bool bSingleMesh)
	{
		if (bSingleMesh)
		{
			return BuildAdjacentPackagePath(FbxFilePath, L"_SkeletalMesh");
		}

		const std::filesystem::path FbxPath = ResolveProjectPath(FbxFilePath);
		const FString FbxStem = FPaths::ToUtf8(FbxPath.stem().wstring());
		const FString NodeStem = SanitizeFileStem(MeshNodeName);
		std::filesystem::path PackagePath = FbxPath;
		PackagePath.replace_filename(FPaths::ToWide(FbxStem + "_" + NodeStem + "_SkeletalMesh.uasset"));
		return NormalizeProjectPath(FPaths::ToUtf8(PackagePath.generic_wstring()));
	}

	TArray<FString> MakeUniqueMeshNames(const TArray<FString>& MeshNames)
	{
		TMap<FString, int32> UsedCounts;
		TArray<FString> UniqueNames;
		UniqueNames.reserve(MeshNames.size());

		for (const FString& MeshName : MeshNames)
		{
			const FString BaseName = SanitizeFileStem(MeshName);
			int32& Count = UsedCounts[BaseName];
			++Count;
			if (Count == 1)
			{
				UniqueNames.push_back(BaseName);
			}
			else
			{
				UniqueNames.push_back(BaseName + "_" + std::to_string(Count));
			}
		}

		return UniqueNames;
	}

	bool SaveSkeletonPackage(USkeleton* Skeleton, const FString& PackagePath, const FString& SourcePath)
	{
		std::filesystem::create_directories(ResolveProjectPath(PackagePath).parent_path());

		FWindowsBinWriter Writer(PackagePath);
		if (!Writer.IsValid())
		{
			UE_LOG("FBX skeleton import package save failed: could not open file. Path=%s", PackagePath.c_str());
			return false;
		}

		try
		{
			FAssetPackageHeader Header;
			Header.Type = static_cast<uint32>(EAssetPackageType::Skeleton);
			Writer << Header;

			FAssetImportMetadata Metadata = MakeImportMetadata(SourcePath);
			Writer << Metadata;

			Skeleton->Serialize(Writer);
		}
		catch (const std::exception&)
		{
			UE_LOG("FBX skeleton import package save failed: serialization threw an exception. Path=%s", PackagePath.c_str());
			return false;
		}

		return Writer.IsValid();
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

	FString SanitizeFileStem(const FString& Name)
	{
		FString Result = Name.empty() ? "None" : Name;
		for (char& Ch : Result)
		{
			const unsigned char U = static_cast<unsigned char>(Ch);
			if (U < 32 || Ch == '<' || Ch == '>' || Ch == ':' || Ch == '"' ||
				Ch == '/' || Ch == '\\' || Ch == '|' || Ch == '?' || Ch == '*')
			{
				Ch = '_';
			}
		}
		return Result.empty() ? FString("None") : Result;
	}

	FString BuildSkeletalMeshPackagePath(const FString& FbxFilePath, const FString& MeshNodeName, bool bSingleMesh)
	{
		if (bSingleMesh)
		{
			return BuildAdjacentPackagePath(FbxFilePath, L"_SkeletalMesh");
		}

		const std::filesystem::path FbxPath = ResolveProjectPath(FbxFilePath);
		const FString FbxStem = FPaths::ToUtf8(FbxPath.stem().wstring());
		const FString NodeStem = SanitizeFileStem(MeshNodeName);
		std::filesystem::path PackagePath = FbxPath;
		PackagePath.replace_filename(FPaths::ToWide(FbxStem + "_" + NodeStem + "_SkeletalMesh.uasset"));
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

FString FEditorFbxImportService::GetSkeletalMeshPackagePathForFbx(const FString& FbxFilePath, const FString& MeshNodeName, bool bSingleMesh)
{
	return BuildSkeletalMeshPackagePath(FbxFilePath, MeshNodeName, bSingleMesh);
}

FString FEditorFbxImportService::GetSkeletonPackagePathForFbx(const FString& FbxFilePath)
{
	return BuildAdjacentPackagePath(FbxFilePath, L"_Skeleton");
}

bool FEditorFbxImportService::DiscoverSkeletalMeshSourcesFromFbx(const FString& FbxFilePath, TArray<FString>& OutPackagePaths)
{
	OutPackagePaths.clear();
#if WITH_EDITOR
	TArray<FString> MeshNames;
	if (!FEditorFbxImporter::DiscoverMeshNames(FbxFilePath, MeshNames))
	{
		return false;
	}

	if (MeshNames.empty())
	{
		return false;
	}

	OutPackagePaths.push_back(GetSkeletalMeshPackagePathForFbx(FbxFilePath));
	return true;
#else
	(void)FbxFilePath;
	return false;
#endif
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

bool FEditorFbxImportService::ImportSkeletalMeshesFromFbx(const FString& FbxFilePath, ID3D11Device* Device, TArray<USkeletalMesh*>& OutSkeletalMeshes, bool bRefreshAssetLists)
{
#if WITH_EDITOR
	OutSkeletalMeshes.clear();

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

	const FString SourcePath = NormalizeProjectPath(FbxFilePath);
	const FString SkeletonPackagePath = GetSkeletonPackagePathForFbx(SourcePath);

	std::unique_ptr<FSkeletonAsset> NewSkeletonAsset = std::make_unique<FSkeletonAsset>();
	NewSkeletonAsset->PathFileName = SourcePath;
	NewSkeletonAsset->Bones = std::move(FEditorFbxImporter::Bones);

	USkeleton* Skeleton = UObjectManager::Get().CreateObject<USkeleton>();
	Skeleton->SetAssetPathFileName(SkeletonPackagePath);
	Skeleton->SetSkeletonAsset(NewSkeletonAsset.release());

	if (!SaveSkeletonPackage(Skeleton, SkeletonPackagePath, SourcePath))
	{
		return false;
	}
	FSkeletonManager::Get().RegisterSkeleton(SkeletonPackagePath, Skeleton);

	TArray<FString> MeshNames;
	MeshNames.reserve(FEditorFbxImporter::ImportedSkeletalMeshes.size());
	for (const FEditorFbxImporter::FImportedSkeletalMesh& ImportedMesh : FEditorFbxImporter::ImportedSkeletalMeshes)
	{
		MeshNames.push_back(ImportedMesh.MeshName);
	}

	const bool bSingleMesh = MeshNames.size() <= 1;
	const TArray<FString> UniqueNames = MakeUniqueMeshNames(MeshNames);

	for (int32 MeshIndex = 0; MeshIndex < static_cast<int32>(FEditorFbxImporter::ImportedSkeletalMeshes.size()); ++MeshIndex)
	{
		FEditorFbxImporter::FImportedSkeletalMesh& ImportedMesh = FEditorFbxImporter::ImportedSkeletalMeshes[MeshIndex];
		const FString MeshName = MeshIndex < static_cast<int32>(UniqueNames.size()) ? UniqueNames[MeshIndex] : ImportedMesh.MeshName;
		const FString PackagePath = GetSkeletalMeshPackagePathForFbx(SourcePath, MeshName, bSingleMesh);

		std::unique_ptr<FSkeletalMesh> NewMeshAsset = std::make_unique<FSkeletalMesh>();
		NewMeshAsset->PathFileName = SourcePath;
		NewMeshAsset->SkeletonPath = SkeletonPackagePath;
		NewMeshAsset->MeshBindGlobal = ImportedMesh.MeshBindGlobal;
		NewMeshAsset->Vertices = std::move(ImportedMesh.Vertices);
		NewMeshAsset->Indices = std::move(ImportedMesh.Indices);
		NewMeshAsset->Sections = std::move(ImportedMesh.Sections);

		USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
		SkeletalMesh->SetSkeletalMaterials(std::move(ImportedMesh.Materials));
		SkeletalMesh->SetSkeletalMeshAsset(NewMeshAsset.release());
		SkeletalMesh->SetSkeleton(Skeleton);

		FMeshManager::SkeletalMeshCache.erase(PackagePath);
		if (!SaveSkeletalMeshPackage(SkeletalMesh, PackagePath, SourcePath))
		{
			return false;
		}

		SkeletalMesh->InitResources(Device);
		SkeletalMesh->SetAssetPathFileName(PackagePath);
		FMeshManager::SkeletalMeshCache[PackagePath] = SkeletalMesh;
		OutSkeletalMeshes.push_back(SkeletalMesh);
	}

	if (bRefreshAssetLists)
	{
		RefreshAssetLists();
	}

	UE_LOG("FBX skeletal meshes imported successfully. Source=%s Skeleton=%s MeshCount=%zu", FbxFilePath.c_str(), SkeletonPackagePath.c_str(), OutSkeletalMeshes.size());
	return !OutSkeletalMeshes.empty();
#else
	(void)FbxFilePath;
	(void)Device;
	(void)bRefreshAssetLists;
	OutSkeletalMeshes.clear();
	return false;
#endif
}

bool FEditorFbxImportService::ImportSkeletalMeshFromFbx(const FString& FbxFilePath, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh, bool bRefreshAssetLists)
{
	OutSkeletalMesh = nullptr;
	TArray<USkeletalMesh*> ImportedMeshes;
	if (!ImportSkeletalMeshesFromFbx(FbxFilePath, Device, ImportedMeshes, bRefreshAssetLists))
	{
		return false;
	}

	OutSkeletalMesh = ImportedMeshes.empty() ? nullptr : ImportedMeshes.front();
	return OutSkeletalMesh != nullptr;
}
