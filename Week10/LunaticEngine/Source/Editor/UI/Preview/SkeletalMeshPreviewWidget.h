#pragma once

#include "AssetPreviewWidget.h"
#include "Core/CoreTypes.h"
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
	void Shutdown();
	void OpenSkeletalMesh(USkeletalMesh* InMesh);
	void Render(float DeltaTime);
	void ClearInputCapture();

	bool IsOpen() const { return bOpen; }
	bool IsCapturingInput() const { return bCapturingInput; }

private:
	void Close();
	void RegisterPreviewClient();
	void UnregisterPreviewClient();
	void DrawBoneHierarchyPanel();
	void DrawBoneDetailsPanel();
	void DrawBoneTreeNode(const FSkeleton& Skeleton, int32 BoneIndex);
	void HandleViewportBoneSelection();
	void ValidateSelectedBone();

private:
	FSkeletalMeshPreviewViewportClient ViewportClient;
	FViewport* Viewport = nullptr;
	UEditorEngine* Engine = nullptr;
	USkeletalMesh* EditingMesh = nullptr;
	int32 SelectedBoneIndex = -1;
	bool bOpen = false;
	bool bInitialized = false;
	bool bRegistered = false;
	bool bCapturingInput = false;
};
