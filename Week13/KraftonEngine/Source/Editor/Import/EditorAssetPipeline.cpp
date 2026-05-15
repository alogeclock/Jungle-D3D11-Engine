#include "Editor/Import/EditorAssetPipeline.h"

#include "Asset/AssetPackage.h"
#include "Core/Log.h"
#include "Editor/Import/EditorFbxImportService.h"
#include "Editor/Import/EditorObjImportService.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Platform/Paths.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

namespace
{
	std::filesystem::path ResolveProjectPath(const FString& Path)
	{
		std::filesystem::path FullPath(FPaths::ToWide(Path));
		if (!FullPath.is_absolute())
		{
			FullPath = std::filesystem::path(FPaths::RootDir()) / FullPath;
		}
		return FullPath.lexically_normal();
	}

	FString NormalizeProjectPath(const FString& Path)
	{
		return FPaths::MakeProjectRelative(Path);
	}

	std::wstring GetLowerExtension(const std::filesystem::path& Path)
	{
		std::wstring Ext = Path.extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		return Ext;
	}

	bool TryGetSourceFileState(const FString& SourcePath, uint64& OutTimestamp, uint64& OutFileSize)
	{
		std::filesystem::path FullPath = ResolveProjectPath(SourcePath);
		if (!std::filesystem::exists(FullPath) || !std::filesystem::is_regular_file(FullPath))
		{
			return false;
		}

		OutFileSize = static_cast<uint64>(std::filesystem::file_size(FullPath));
		const auto WriteTime = std::filesystem::last_write_time(FullPath);
		OutTimestamp = static_cast<uint64>(WriteTime.time_since_epoch().count());
		return true;
	}

	bool IsPackageCurrent(const FString& SourcePath, const FString& PackagePath, EAssetPackageType ExpectedType)
	{
		if (!std::filesystem::exists(ResolveProjectPath(PackagePath)))
		{
			return false;
		}

		FAssetImportMetadata Metadata;
		if (!FAssetPackage::ReadMetadata(PackagePath, ExpectedType, Metadata))
		{
			return false;
		}

		if (NormalizeProjectPath(Metadata.SourcePath) != SourcePath)
		{
			return false;
		}

		uint64 CurrentTimestamp = 0;
		uint64 CurrentFileSize = 0;
		if (!TryGetSourceFileState(SourcePath, CurrentTimestamp, CurrentFileSize))
		{
			return false;
		}

		return Metadata.MatchesSource(CurrentTimestamp, CurrentFileSize);
	}

	bool IsStaticMeshPackageCurrentForObj(const FString& ObjPath)
	{
		const FString SourcePath = NormalizeProjectPath(ObjPath);
		const FString PackagePath = FEditorObjImportService::GetStaticMeshPackagePathForObj(SourcePath);
		return IsPackageCurrent(SourcePath, PackagePath, EAssetPackageType::StaticMesh);
	}

	bool IsStaticMeshPackageCurrentForFbx(const FString& FbxPath)
	{
		const FString SourcePath = NormalizeProjectPath(FbxPath);
		const FString PackagePath = FEditorFbxImportService::GetStaticMeshPackagePathForFbx(SourcePath);
		return IsPackageCurrent(SourcePath, PackagePath, EAssetPackageType::StaticMesh);
	}

	bool IsSkeletalMeshPackageCurrentForFbx(const FString& FbxPath)
	{
		const FString SourcePath = NormalizeProjectPath(FbxPath);
		const FString PackagePath = FEditorFbxImportService::GetSkeletalMeshPackagePathForFbx(SourcePath);
		return IsPackageCurrent(SourcePath, PackagePath, EAssetPackageType::SkeletalMesh);
	}
}

FEditorAssetPipelineReport FEditorAssetPipeline::SyncAssetRoot(ID3D11Device* Device)
{
	FEditorAssetPipelineReport Report;

	if (!Device)
	{
		UE_LOG("Editor asset pipeline sync skipped: D3D device is null.");
		return Report;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	const std::filesystem::path AssetRoot(FPaths::AssetDir());
	if (!std::filesystem::exists(AssetRoot))
	{
		UE_LOG("Editor asset pipeline sync skipped: Asset root does not exist.");
		return Report;
	}

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		const std::filesystem::path& Path = Entry.path();
		const std::wstring Extension = GetLowerExtension(Path);
		if (Extension != L".obj" && Extension != L".fbx")
		{
			continue;
		}

		++Report.Scanned;

		const FString SourcePath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		if (Extension == L".obj")
		{
			if (IsStaticMeshPackageCurrentForObj(SourcePath))
			{
				++Report.Skipped;
				continue;
			}

			UStaticMesh* ImportedMesh = nullptr;
			if (FEditorObjImportService::ImportStaticMeshFromObj(SourcePath, FImportOptions::Default(), Device, ImportedMesh, false))
			{
				++Report.Imported;
			}
			else
			{
				++Report.Failed;
			}

			continue;
		}

		const bool bStaticCurrent = IsStaticMeshPackageCurrentForFbx(SourcePath);
		const bool bSkeletalCurrent = IsSkeletalMeshPackageCurrentForFbx(SourcePath);
		const bool bStaticMissingOrStale = !bStaticCurrent;
		const bool bSkeletalPackageExists = std::filesystem::exists(ResolveProjectPath(FEditorFbxImportService::GetSkeletalMeshPackagePathForFbx(SourcePath)));
		const bool bSkeletalMissingOrStale = !bSkeletalCurrent && (bStaticMissingOrStale || bSkeletalPackageExists);

		if (!bStaticMissingOrStale && !bSkeletalMissingOrStale)
		{
			++Report.Skipped;
			continue;
		}

		if (bStaticMissingOrStale)
		{
			UStaticMesh* ImportedMesh = nullptr;
			if (FEditorFbxImportService::ImportStaticMeshFromFbx(SourcePath, FImportOptions::Default(), Device, ImportedMesh, false))
			{
				++Report.Imported;
			}
			else
			{
				++Report.Failed;
			}
		}

		if (bSkeletalMissingOrStale)
		{
			USkeletalMesh* ImportedMesh = nullptr;
			if (FEditorFbxImportService::ImportSkeletalMeshFromFbx(SourcePath, Device, ImportedMesh, false))
			{
				++Report.Imported;
			}
			else
			{
				++Report.Failed;
			}
		}
	}

	if (Report.Imported > 0)
	{
		FMeshManager::ScanMeshAssets();
		FMaterialManager::Get().ScanMaterialAssets();
	}

	UE_LOG(
		"Editor asset pipeline sync completed. Scanned=%u Imported=%u Skipped=%u Failed=%u",
		Report.Scanned,
		Report.Imported,
		Report.Skipped,
		Report.Failed);

	return Report;
}
