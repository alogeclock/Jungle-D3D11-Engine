#pragma once

#include "AssetPreviewWidget.h"
#include "Core/CoreTypes.h"
#include "Editor/UI/EditorDetailsWidget.h"
#include "Editor/Viewport/Preview/SkeletalMeshPreviewViewportClient.h"

class FViewport;
class FWindowsWindow;
class UEditorEngine;
class USkeletalMesh;
struct FSkeleton;
struct ID3D11Device;

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
	void HandleViewportBoneSelection();
	void ValidateSelectedBone();

private:
	FSkeletalMeshPreviewViewportClient ViewportClient;
	FEditorDetailsWidget ComponentDetailsWidget;
	FViewport* Viewport = nullptr;
	UEditorEngine* Engine = nullptr;
	USkeletalMesh* EditingMesh = nullptr;
	int32 SelectedBoneIndex = -1;
	int32 EditorInstanceId = 0;
	uint32 PreviewDockId = 0;
	bool bOpen = false;
	bool bInitialized = false;
	bool bRegistered = false;
	bool bCapturingInput = false;
};
