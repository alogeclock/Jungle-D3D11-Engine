#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/MeshImportOptions.h"
#include "Mesh/MeshManager.h"

class UStaticMesh;
class USkeletalMesh;
class USkeleton;
class UAnimSequence;
struct ID3D11Device;

// Editor-only FBX import path. Runtime code should load cooked .uasset files through FMeshManager.
struct FEditorFbxImportService
{
	static FString GetStaticMeshPackagePathForFbx(const FString& FbxFilePath);
	static FString GetSkeletalMeshPackagePathForFbx(const FString& FbxFilePath);
	static FString GetSkeletalMeshPackagePathForFbx(const FString& FbxFilePath, const FString& MeshNodeName, bool bSingleMesh);
	static FString GetSkeletonPackagePathForFbx(const FString& FbxFilePath);
	static FString GetAnimSequencePackagePathForFbx(const FString& FbxFilePath);
	static FString GetAnimSequencePackagePathForFbx(const FString& FbxFilePath, const FString& AnimStackName, bool bSingleStack);
	static bool DiscoverSkeletalMeshSourcesFromFbx(const FString& FbxFilePath, TArray<FString>& OutPackagePaths);
	static bool DiscoverAnimSequenceSourcesFromFbx(const FString& FbxFilePath, TArray<FString>& OutPackagePaths);

	static bool ImportStaticMeshFromFbx(const FString& FbxFilePath, ID3D11Device* Device, UStaticMesh*& OutStaticMesh, bool bRefreshAssetLists = true);
	static bool ImportStaticMeshFromFbx(const FString& FbxFilePath, const FImportOptions& Options, ID3D11Device* Device, UStaticMesh*& OutStaticMesh, bool bRefreshAssetLists = true);
	static bool ImportSkeletalMeshesFromFbx(const FString& FbxFilePath, ID3D11Device* Device, TArray<USkeletalMesh*>& OutSkeletalMeshes, bool bRefreshAssetLists = true);
	static bool ImportSkeletalMeshFromFbx(const FString& FbxFilePath, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh, bool bRefreshAssetLists = true);
	static bool ImportAnimSequencesFromFbx(const FString& FbxFilePath, TArray<UAnimSequence*>& OutAnimSequences, bool bRefreshAssetLists = true);
	static bool ImportAnimSequencesFromFbx(const FString& FbxFilePath, const FString& TargetSkeletonPath, TArray<UAnimSequence*>& OutAnimSequences, bool bRefreshAssetLists = true);

	static void ScanFbxSourceFiles();
	static const TArray<FMeshAssetListItem>& GetAvailableFbxFiles() { return AvailableFbxFiles; }

private:
	static TArray<FMeshAssetListItem> AvailableFbxFiles;
};
