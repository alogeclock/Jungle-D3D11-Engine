#pragma once
#include "Editor/UI/EditorWidget.h"
#include "ContentItem.h"
#include <d3d11.h>
#include <memory>
#include "ContentBrowserContext.h"
#include "ContentBrowserElement.h"
#include "Mesh/MeshManager.h"

class FEditorContentBrowserWidget final : public FEditorWidget
{
	struct FDirNode
	{
		FContentItem Self;
		TArray<FDirNode> Children;
	};

public:
	void Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice);
	void Render(float DeltaTime) override;
	void Refresh();
	void SaveToSettings() const;
	void SetIconSize(float Size);
	float GetIconSize() const { return BrowserContext.ContentSize.x; }

private:
	void LoadFromSettings();
	void RefreshContent();
	void DrawDirNode(FDirNode InNode);
	void DrawContents();
	void BeginImportSourceFile();
	void BeginFbxImport(const FString& SourcePath);
	void RenderFbxImportPopup();
	bool ExecuteObjImport(const FString& SourcePath);
	bool ExecuteFbxImport();
	void RefreshImportedAssetLists();

	TArray<FContentItem> ReadDirectory(std::wstring Path);
	FDirNode BuildDirectoryTree(const std::filesystem::path& DirPath);
	TArray<FMeshAssetListItem> ScanSkeletonAssets() const;

private:
	ContentBrowserContext BrowserContext;

	FDirNode RootNode;
	TArray<std::shared_ptr<ContentBrowserElement>> CachedBrowserElements;
	TMap<FString, std::wstring> IconFileMap;

	FString PendingImportSourcePath;
	bool bOpenFbxImportPopup = false;
	bool bImportFbxStaticMesh = true;
	bool bImportFbxSkeletalMesh = true;
	bool bImportFbxAnimations = true;
	int32 PendingStaticFbxSkinnedMeshPolicy = 1;
	int32 SelectedTargetSkeletonIndex = 0;
	TArray<FMeshAssetListItem> TargetSkeletonOptions;
};
