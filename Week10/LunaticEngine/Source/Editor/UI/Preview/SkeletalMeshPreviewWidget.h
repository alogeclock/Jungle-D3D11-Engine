#pragma once

#include "AssetPreviewWidget.h"
#include "Core/CoreTypes.h"
#include "Editor/UI/EditorDetailsWidget.h"
#include "Editor/Viewport/Preview/SkeletalMeshPreviewViewportClient.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"

class FWindowsWindow;
class UEditorEngine;
class USkeletalMesh;
class UTexture2D;
struct FSkeleton;

enum class EBoneRotationEditSpace : uint8
{
	Local,
	Component
};

struct FBoneRotationEditState
{
	int32 BoneIndex = -1;
	FQuat Quat = FQuat::Identity;
	FRotator EulerHint = FRotator();
};

class FSkeletalMeshPreviewWidget : public FAssetPreviewWidget
{
public:
	void Initialize(UEditorEngine* InEngine, ID3D11Device* InDevice, FWindowsWindow* InWindow);
	void Shutdown() override;
	void OpenSkeletalMesh(USkeletalMesh* InMesh);
	void Render(float DeltaTime) override;
	void SetEditorInstanceId(int32 InInstanceId) { EditorInstanceId = InInstanceId; }

private:
	void Close();
	void DrawBoneHierarchyPanel();
	void DrawBoneDetailsPanel();
	void DrawPreviewComponentDetailsPanel();
	void DrawBoneTreeNode(const FSkeleton& Skeleton, int32 BoneIndex);
	bool DrawEditableTransform(const char* SectionName, FTransform& Transform, int32 BoneIndex, FBoneRotationEditState& State, EBoneRotationEditSpace Space);
	void RebuildBoneWeightedFlags();
	void EnsureBoneTreeIcons();
	void HandleViewportBoneSelection();
	void ValidateSelectedBone();

private:
	FSkeletalMeshPreviewViewportClient ViewportClient;
	FEditorDetailsWidget ComponentDetailsWidget;
	USkeletalMesh* EditingMesh = nullptr;
	UTexture2D* WeightedBoneIcon = nullptr;
	UTexture2D* NonWeightedBoneIcon = nullptr;
	TArray<bool> BoneWeightedFlags;
	int32 SelectedBoneIndex = -1;
	FBoneRotationEditState LocalRotationEditState;
	FBoneRotationEditState ComponentRotationEditState;
	bool bInitialized = false;
};
