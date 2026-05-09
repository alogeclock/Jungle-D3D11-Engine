#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshBake.h"
#include "Materials/Material.h"
#include "Core/Log.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include <filesystem>
#include <algorithm>

#include "FbxImporter.h"

TMap<FString, UStaticMesh*> FObjManager::StaticMeshCache;
TArray<FMeshAssetListItem> FObjManager::AvailableMeshFiles;
TArray<FMeshAssetListItem> FObjManager::AvailableObjFiles;

namespace
{
	std::filesystem::path GetMeshCacheDirectory(const FString& OriginalPath)
	{
		std::filesystem::path SourcePath(FPaths::ToWide(OriginalPath));
		return SourcePath.parent_path() / L"Cache";
	}

	void EnsureMeshCacheDirExists(const FString& OriginalPath)
	{
		FPaths::CreateDir(GetMeshCacheDirectory(OriginalPath).wstring());
	}
}

FString FObjManager::GetObjBinaryFilePath(const FString& OriginalPath)
{
	std::filesystem::path SrcPath(FPaths::ToWide(OriginalPath));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

	// 이미 cache bin 경로가 들어온 경우에는 그대로 사용한다.
	if (Ext == L".bin")
	{
		return OriginalPath;
	}

	std::filesystem::path BinPath = GetMeshCacheDirectory(OriginalPath) / SrcPath.stem();
	BinPath += L".obj.v3.bin";
	return FPaths::ToUtf8(BinPath.lexically_normal().generic_wstring());
}

FString FObjManager::GetFbxStaticBinaryFilePath(const FString& OriginalPath)
{
	std::filesystem::path SrcPath(FPaths::ToWide(OriginalPath));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

	// 이미 cache bin 경로가 들어온 경우에는 그대로 사용한다.
	if (Ext == L".bin")
	{
		return OriginalPath;
	}

	std::filesystem::path BinPath = GetMeshCacheDirectory(OriginalPath) / SrcPath.stem();
	BinPath += L".fbxstatic.v3.bin";
	return FPaths::ToUtf8(BinPath.lexically_normal().generic_wstring());
}

FString FObjManager::GetBinaryFilePath(const FString& OriginalPath)
{
	return GetObjBinaryFilePath(OriginalPath);
}



void FObjManager::ScanMeshAssets()
{
	AvailableMeshFiles.clear();

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	const std::filesystem::path ContentRoot = FPaths::ContentDir();
	if (!std::filesystem::exists(ContentRoot))
	{
		return;
	}

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(ContentRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		if (Path.extension() != L".bin") continue;
		if (_wcsicmp(Path.parent_path().filename().c_str(), L"Cache") != 0) continue;

		std::wstring DisplayStem = Path.stem().wstring();
		const std::wstring ObjSuffix = L".obj.v3";
		const std::wstring FbxSuffix = L".fbxstatic.v3";
		if (DisplayStem.size() > ObjSuffix.size() && DisplayStem.compare(DisplayStem.size() - ObjSuffix.size(), ObjSuffix.size(), ObjSuffix) == 0)
		{
			DisplayStem.erase(DisplayStem.size() - ObjSuffix.size());
		}
		else if (DisplayStem.size() > FbxSuffix.size() && DisplayStem.compare(DisplayStem.size() - FbxSuffix.size(), FbxSuffix.size(), FbxSuffix) == 0)
		{
			DisplayStem.erase(DisplayStem.size() - FbxSuffix.size());
		}

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(DisplayStem);
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableMeshFiles.push_back(std::move(Item));
	}
}

void FObjManager::ScanObjSourceFiles()
{
	AvailableObjFiles.clear();

	const std::filesystem::path DataRoot = FPaths::ContentDir();

	if (!std::filesystem::exists(DataRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());


	for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();

		// 대소문자 무시
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".obj" && Ext != L".fbx") continue;

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableObjFiles.push_back(std::move(Item));
	}
}

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableMeshFiles()
{
	return AvailableMeshFiles;
}

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableObjFiles()
{
	return AvailableObjFiles;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
	FString CacheKey = GetObjBinaryFilePath(PathFileName);

	// 옵션이 다를 수 있으므로 기존 메모리 캐시는 무효화하고 항상 리빌드한다.
	StaticMeshCache.erase(CacheKey);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	FStaticMesh* NewMeshAsset = new FStaticMesh();
	TArray<FStaticMaterial> ParsedMaterials;

	if (!FObjImporter::Import(PathFileName, Options, *NewMeshAsset, ParsedMaterials))
	{
		delete NewMeshAsset;
		return nullptr;
	}

	NewMeshAsset->PathFileName = PathFileName;
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset);

	EnsureMeshCacheDirExists(PathFileName);
	StaticMeshBake::Save(CacheKey, *StaticMesh, StaticMeshBake::ESourceKind::Obj);

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

UStaticMesh* FObjManager::LoadFbxStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	FString CacheKey = GetFbxStaticBinaryFilePath(PathFileName);

	auto It = StaticMeshCache.find(CacheKey);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath      = CacheKey;
	bool    bNeedRebuild = true;

	const std::filesystem::path SourcePathW(FPaths::ToWide(PathFileName));
	const std::filesystem::path BinPathW(FPaths::ToWide(BinPath));

	if (std::filesystem::exists(BinPathW))
	{
		const bool bCacheIsFresh = !std::filesystem::exists(SourcePathW) ||
			std::filesystem::last_write_time(BinPathW) >= std::filesystem::last_write_time(SourcePathW);

		if (bCacheIsFresh && StaticMeshBake::Load(BinPath, *StaticMesh, StaticMeshBake::ESourceKind::FbxStatic))
		{
			bNeedRebuild = false;
		}
	}

	if (bNeedRebuild)
	{
		FStaticMesh*            NewMeshAsset = new FStaticMesh();
		TArray<FStaticMaterial> ParsedMaterials;

		if (!FFbxImporter::ImportStaticMesh(PathFileName, *NewMeshAsset, ParsedMaterials))
		{
			delete NewMeshAsset;
			return nullptr;
		}

		NewMeshAsset->PathFileName = PathFileName;
		StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
		StaticMesh->SetStaticMeshAsset(NewMeshAsset);

		EnsureMeshCacheDirExists(PathFileName);
		StaticMeshBake::Save(BinPath, *StaticMesh, StaticMeshBake::ESourceKind::FbxStatic);
	}

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	const std::filesystem::path InitialPathW(FPaths::ToWide(PathFileName));
	std::wstring InitialExt = InitialPathW.extension().wstring();
	std::transform(InitialExt.begin(), InitialExt.end(), InitialExt.begin(), ::towlower);

	if (InitialExt == L".fbx")
	{
		return LoadFbxStaticMesh(PathFileName, InDevice);
	}

	FString CacheKey = GetObjBinaryFilePath(PathFileName);

	// BinPath 기반 캐시 확인
	auto It = StaticMeshCache.find(CacheKey);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath = CacheKey;
	bool bNeedRebuild = true;
	FString ObjPath = PathFileName;
	const std::filesystem::path RequestedPathW(FPaths::ToWide(PathFileName));
	std::wstring RequestedExt = RequestedPathW.extension().wstring();
	std::transform(RequestedExt.begin(), RequestedExt.end(), RequestedExt.begin(), ::towlower);

	// .bin 직접 선택 시에는 header가 있는 새 static bake를 먼저 읽고, 실패하면 구 legacy .bin을 한 번만 시도한다.
	if (RequestedExt == L".bin")
	{
		if (StaticMeshBake::Load(BinPath, *StaticMesh, StaticMeshBake::ESourceKind::Unknown))
		{
			bNeedRebuild = false;
		}
		else
		{
			FWindowsBinReader LegacyReader(BinPath);
			if (LegacyReader.IsValid())
			{
				StaticMesh->Serialize(LegacyReader);
				bNeedRebuild = false;
			}
		}

		if (!bNeedRebuild && StaticMesh->GetStaticMeshAsset() && !StaticMesh->GetStaticMeshAsset()->PathFileName.empty())
		{
			ObjPath = StaticMesh->GetStaticMeshAsset()->PathFileName;
			const std::filesystem::path ObjPathW(FPaths::ToWide(ObjPath));
			const std::filesystem::path BinPathW(FPaths::ToWide(BinPath));
			if (std::filesystem::exists(ObjPathW) && std::filesystem::exists(BinPathW) &&
				std::filesystem::last_write_time(ObjPathW) > std::filesystem::last_write_time(BinPathW))
			{
				bNeedRebuild = true;
			}
		}
	}

	// OBJ 경로로 들어온 경우 타임스탬프와 static bake header를 함께 검증한다.
	std::filesystem::path BinPathW(FPaths::ToWide(BinPath));
	std::filesystem::path PathFileNameW(FPaths::ToWide(PathFileName));
	if (RequestedExt != L".bin" && std::filesystem::exists(BinPathW))
	{
		const bool bCacheIsFresh = !std::filesystem::exists(PathFileNameW) ||
			std::filesystem::last_write_time(BinPathW) >= std::filesystem::last_write_time(PathFileNameW);

		if (bCacheIsFresh && StaticMeshBake::Load(BinPath, *StaticMesh, StaticMeshBake::ESourceKind::Obj))
		{
			bNeedRebuild = false;
		}
	}

	if (bNeedRebuild)
	{
		const std::filesystem::path SourcePathW(FPaths::ToWide(ObjPath));
		std::wstring SourceExt = SourcePathW.extension().wstring();
		std::transform(SourceExt.begin(), SourceExt.end(), SourceExt.begin(), ::towlower);

		if (SourceExt == L".fbx")
		{
			return LoadFbxStaticMesh(ObjPath, InDevice);
		}

		FStaticMesh* NewMeshAsset = new FStaticMesh();
		TArray<FStaticMaterial> ParsedMaterials;

		if (!FObjImporter::Import(ObjPath, *NewMeshAsset, ParsedMaterials))
		{
			delete NewMeshAsset;
			return nullptr;
		}

		NewMeshAsset->PathFileName = ObjPath;
		StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
		StaticMesh->SetStaticMeshAsset(NewMeshAsset);

		EnsureMeshCacheDirExists(ObjPath);
		StaticMeshBake::Save(GetObjBinaryFilePath(ObjPath), *StaticMesh, StaticMeshBake::ESourceKind::Obj);
	}

	StaticMesh->InitResources(InDevice);

	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}


void FObjManager::ReleaseAllGPU()
{
	for (auto& [Key, Mesh] : StaticMeshCache)
	{
		if (Mesh)
		{
			FStaticMesh* Asset = Mesh->GetStaticMeshAsset();
			if (Asset && Asset->RenderBuffer)
			{
				Asset->RenderBuffer->Release();
				Asset->RenderBuffer.reset();
			}
			// LOD 버퍼도 해제
			for (uint32 LOD = 1; LOD < UStaticMesh::MAX_LOD_COUNT; ++LOD)
			{
				FMeshBuffer* LODBuffer = Mesh->GetLODMeshBuffer(LOD);
				if (LODBuffer)
				{
					LODBuffer->Release();
				}
			}
		}
	}
	StaticMeshCache.clear();
}
