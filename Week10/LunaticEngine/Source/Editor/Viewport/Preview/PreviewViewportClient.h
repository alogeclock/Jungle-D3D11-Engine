#pragma once

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/Preview/PreviewScene.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputAction.h"
#include "Input/InputMappingContext.h"
#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"
#include "UI/SWindow.h"

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

	void Initialize(FWindowsWindow* InWindow);
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
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
	float GetPreviewCameraSpeed() const { return PreviewCameraSpeed; }
	void SetPreviewCameraSpeed(float InSpeed);

protected:
	virtual void OnCameraReset() {}

private:
	void SetupInput();
	void TickInput(const FInputSystemSnapshot& Snapshot, float DeltaTime);

	void OnMove(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnRotate(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnPan(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnZoom(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnOrbit(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);

	void SyncCamera();
	void UpdateCameraPosition(float DeltaTime);
	void ApplyOrbitInput();

private:
	FPreviewScene PreviewScene;

	bool bSuppressInputUntilMouseUp = false;

	FVector OrbitTarget = FVector::ZeroVector;
	float PreviewCameraSpeed = 10.0f;

	FInputMappingContext* PreviewMappingContext = nullptr;

	FInputAction* ActionPreviewMove = nullptr;
	FInputAction* ActionPreviewRotate = nullptr;
	FInputAction* ActionPreviewPan = nullptr;
	FInputAction* ActionPreviewZoom = nullptr;
	FInputAction* ActionPreviewOrbit = nullptr;

	FVector MoveDelta = FVector::ZeroVector;
	FVector RotateDelta = FVector::ZeroVector;
	FVector PanDelta = FVector::ZeroVector;
	FVector OrbitDelta = FVector::ZeroVector;
	float ZoomDelta = 0.0f;
};
