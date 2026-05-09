#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

#include <filesystem>

struct ImVec2;
class FEditorSkeletalMeshViewer final : public FEditorWidget
{
public:
	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;

	struct FSkeletalMeshAssetItem
	{
		FString DisplayName;
		std::filesystem::path Path;
	};

	struct FPreviewBone
	{
		FString Name;
		int32 ParentIndex = -1;
		FVector LocalTranslation = FVector::ZeroVector;
		FRotator LocalRotation;
		FVector LocalScale = FVector(1.0f, 1.0f, 1.0f);
	};

private:
	void RefreshAssetList();
	void OpenAsset(int32 AssetIndex);
	void BuildFallbackSkeleton(const FString& AssetName);

	void RenderToolbar();
	void RenderAssetList();
	void RenderPreview();
	void RenderBoneHierarchy();
	void RenderBoneNode(int32 BoneIndex);
	void RenderSelectedBoneDetails();
	void RenderSkeletonOverlay(const ImVec2& Origin, const ImVec2& Size);

private:
	TArray<FSkeletalMeshAssetItem> AssetItems;
	TArray<FPreviewBone> PreviewBones;
	std::filesystem::path OpenAssetPath;
	int32 SelectedAssetIndex = -1;
	int32 SelectedBoneIndex = -1;
	bool bAutoRefreshRequested = true;
};
