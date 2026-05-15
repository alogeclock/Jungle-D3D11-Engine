#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/MeshImportOptions.h"
#include "Mesh/MeshManager.h"

class UStaticMesh;
class USkeletalMesh;
struct ID3D11Device;

// Editor/ObjViewer-only FBX import path. Runtime code should load cooked .uasset files through FMeshManager.
struct FEditorFbxImportService
{
	static FString GetStaticMeshPackagePathForFbx(const FString& FbxFilePath);
	static FString GetSkeletalMeshPackagePathForFbx(const FString& FbxFilePath);

	static bool ImportStaticMeshFromFbx(const FString& FbxFilePath, ID3D11Device* Device, UStaticMesh*& OutStaticMesh, bool bRefreshAssetLists = true);
	static bool ImportStaticMeshFromFbx(const FString& FbxFilePath, const FImportOptions& Options, ID3D11Device* Device, UStaticMesh*& OutStaticMesh, bool bRefreshAssetLists = true);
	static bool ImportSkeletalMeshFromFbx(const FString& FbxFilePath, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh, bool bRefreshAssetLists = true);

	static void ScanFbxSourceFiles();
	static const TArray<FMeshAssetListItem>& GetAvailableFbxFiles() { return AvailableFbxFiles; }

private:
	static TArray<FMeshAssetListItem> AvailableFbxFiles;
};
