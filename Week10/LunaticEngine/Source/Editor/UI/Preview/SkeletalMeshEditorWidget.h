#pragma once

#include "AssetPreviewWidget.h"
#include "Core/CoreTypes.h"
#include "Editor/UI/Preview/PreviewViewportWidget.h"
#include "Editor/Viewport/Preview/SkeletalMeshPreviewViewportClient.h"

class FViewport;
class FWindowsWindow;
class UEditorEngine;
class USkeletalMesh;
struct ID3D11Device;

class FSkeletalMeshEditorWidget : public FAssetPreviewWidget
{
public:
	void Initialize(UEditorEngine* InEditorEngine, ID3D11Device* InDevice, FWindowsWindow* InWindow);
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
	FString MakeWindowTitle() const;

private:
	FPreviewViewportWidget ViewportWidget;
	FSkeletalMeshPreviewViewportClient ViewportClient;
	FViewport* Viewport = nullptr;
	UEditorEngine* EditorEngine = nullptr;
	USkeletalMesh* EditingMesh = nullptr;
	bool bOpen = false;
	bool bInitialized = false;
	bool bRegistered = false;
	bool bCapturingInput = false;
};
