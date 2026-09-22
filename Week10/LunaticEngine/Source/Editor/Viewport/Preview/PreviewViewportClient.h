#pragma once

#include "Editor/Settings/PreviewSettings.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/Preview/PreviewScene.h"
#include "Input/InputAction.h"
#include "Input/InputMappingContext.h"
#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"

class FViewport;
class FWindowsWindow;
class UCameraComponent;
class UWorld;

// 에셋 프리뷰 뷰포트에서 공통으로 쓰는 카메라와 입력 처리 클라이언트입니다.
// orbit, pan, zoom, focus 같은 기본 조작을 제공하고 에셋별 클라이언트가 상속합니다.
class FPreviewViewportClient : public FEditorViewportClient
{
public:
	FPreviewViewportClient();
	~FPreviewViewportClient() override;

	void Initialize(FWindowsWindow* InWindow) override;
	void CreateCamera() override;
	void DestroyCamera() override;
	void ResetCamera() override;
	void Tick(float DeltaTime) override;
	bool HandleInputSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime) override;

	UWorld* GetWorld() const { return PreviewScene.GetWorld(); }
	FPreviewScene& GetPreviewScene() { return PreviewScene; }
	const FPreviewScene& GetPreviewScene() const { return PreviewScene; }

	void SetupFrameContext(FFrameContext& OutFrame, UCameraComponent* InCamera, FViewport* InVP, UWorld* InWorld) override;
	void SetViewportRect(float X, float Y, float Width, float Height);

	void SetOrbitTarget(const FVector& InTarget);
	const FVector& GetOrbitTarget() const { return OrbitTarget; }
	bool FocusBounds(const FVector& Center, const FVector& Extent);
	void SetViewportType(ELevelViewportType NewType);
	float GetPreviewCameraSpeed() const { return PreviewSettings.CameraSpeed; }
	void SetPreviewCameraSpeed(float InSpeed);
	FPreviewSettings& GetPreviewSettings() { return PreviewSettings; }
	const FPreviewSettings& GetPreviewSettings() const { return PreviewSettings; }
	void MarkPreviewSettingsDirty() { bPreviewSettingsDirty = true; }
	virtual void SetPreviewGizmoMode(int32 InMode) { (void)InMode; }
	virtual int32 GetPreviewGizmoMode() const { return 0; }
	virtual void TogglePreviewGizmoMode();

protected:
	virtual void OnCameraReset() {}
	void TickInput(const FInputSystemSnapshot& Snapshot, float DeltaTime) override;
	
private:
	void SetupInput() override;

	void OnMove(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot) override;
	void OnRotate(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot) override;
	void OnPan(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot) override;
	void OnZoom(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot) override;
	virtual void OnOrbit(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	virtual void OnToggleGizmoMode(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);

	void SyncCamera();
	void UpdateCameraPosition(float DeltaTime);
	void ApplyOrbitInput();
	void SavePreviewSettings();

private:
	FPreviewScene PreviewScene;
	FPreviewSettings PreviewSettings;

	bool bSuppressInputUntilMouseUp = false;
	bool bPreviewSettingsDirty = false;

	FVector OrbitTarget = FVector::ZeroVector;

	FInputMappingContext* PreviewMappingContext = nullptr;

	FInputAction* ActionPreviewMove = nullptr;
	FInputAction* ActionPreviewRotate = nullptr;
	FInputAction* ActionPreviewPan = nullptr;
	FInputAction* ActionPreviewZoom = nullptr;
	FInputAction* ActionPreviewOrbit = nullptr;
	FInputAction* ActionPreviewToggleGizmo = nullptr;

	FVector MoveDelta = FVector::ZeroVector;
	FVector RotateDelta = FVector::ZeroVector;
	FVector PanDelta = FVector::ZeroVector;
	FVector OrbitDelta = FVector::ZeroVector;
	float ZoomDelta = 0.0f;
};
