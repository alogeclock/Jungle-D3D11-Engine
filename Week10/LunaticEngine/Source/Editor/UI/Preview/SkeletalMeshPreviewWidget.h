#pragma once

#include "AssetPreviewWidget.h"
#include "Core/CoreTypes.h"
#include "Editor/UI/EditorDetailsWidget.h"
#include "Editor/Viewport/Preview/SkeletalMeshPreviewViewportClient.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"

class FViewport;
class FWindowsWindow;
class UEditorEngine;
class USkeletalMesh;
class UTexture2D;
struct FSkeleton;
struct ID3D11Device;

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
	void ClearInputCapture() override;
	void SetDockId(uint32 InDockId) override { PreviewDockId = InDockId; }
	void SetEditorInstanceId(int32 InInstanceId) { EditorInstanceId = InInstanceId; }

	bool IsOpen() const override { return bOpen; }
	bool IsCapturingInput() const override { return bCapturingInput; }

private:
	void Close();
	void RegisterPreviewClient();
	void UnregisterPreviewClient();
	void DrawBoneHierarchyPanel();
	void DrawBoneDetailsPanel();
	void DrawPreviewComponentDetailsPanel();
	void DrawBoneTreeNode(const FSkeleton& Skeleton, int32 BoneIndex);
	void RebuildBoneWeightedFlags();
	void EnsureBoneTreeIcons();
	void HandleViewportBoneSelection();
	void ValidateSelectedBone();

private:
	FSkeletalMeshPreviewViewportClient ViewportClient;
	FEditorDetailsWidget ComponentDetailsWidget;
	FViewport* Viewport = nullptr;
	UEditorEngine* Engine = nullptr;
	ID3D11Device* Device = nullptr;
	USkeletalMesh* EditingMesh = nullptr;
	UTexture2D* WeightedBoneIcon = nullptr;
	UTexture2D* NonWeightedBoneIcon = nullptr;
	TArray<bool> BoneWeightedFlags;
	int32 SelectedBoneIndex = -1;
	int32 EditorInstanceId = 0;
	uint32 PreviewDockId = 0;
	FBoneRotationEditState LocalRotationEditState;
	FBoneRotationEditState ComponentRotationEditState;
	bool bOpen = false;
	bool bInitialized = false;
	bool bRegistered = false;
	bool bCapturingInput = false;
};
