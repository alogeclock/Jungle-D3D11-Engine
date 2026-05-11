#pragma once

#include "Viewport/ViewportClient.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"

#include "Core/CollisionTypes.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputAction.h"
#include "Input/InputMappingContext.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "UI/SWindow.h"

class FEditorSettings;
class FViewport;
class FWindowsWindow;
class UCameraComponent;
class UWorld;
struct FFrameContext;

class FEditorViewportClient : public FViewportClient
{
public:
	explicit FEditorViewportClient(bool bSetupDefaultInput = true);
	~FEditorViewportClient() override;

	void Initialize(FWindowsWindow* InWindow);
	void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }

	virtual void SetupFrameContext(FFrameContext& OutFrame, UCameraComponent* InCamera, FViewport* InVP, UWorld* InWorld) = 0;

	FViewportRenderOptions& GetRenderOptions() { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const { return RenderOptions; }

	void SetViewportSize(float InWidth, float InHeight);

	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	UCameraComponent* GetCamera() const { return Camera; }

	virtual void Tick(float DeltaTime);
	bool HandleInputSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime) override;

	void SetActive(bool bInActive) { bIsActive = bInActive; }
	bool IsActive() const { return bIsActive; }

	void SetHovered(bool bInHovered) { bIsHovered = bInHovered; }
	bool IsHovered() const { return bIsHovered; }

	void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	FViewport* GetViewport() const { return Viewport; }

	const FRect& GetViewportScreenRect() const { return ViewportScreenRect; }
	bool GetCursorViewportPosition(uint32& OutX, uint32& OutY) const;

protected:
	virtual bool CanProcessCameraInput() const { return true; }

	void TickInput(const FInputSystemSnapshot& Snapshot, float DeltaTime);
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);

protected:
	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	UCameraComponent* Camera = nullptr;
	const FEditorSettings* Settings = nullptr;
	FViewportRenderOptions RenderOptions;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	bool bIsActive = false;
	bool bIsHovered = false;
	FRect ViewportScreenRect;

	bool bIsFocusAnimating = false;
	FVector FocusStartLoc;
	FRotator FocusStartRot;
	FVector FocusEndLoc;
	FRotator FocusEndRot;
	float FocusAnimTimer = 0.0f;
	const float FocusAnimDuration = 0.5f;

	FVector TargetLocation;
	bool bTargetLocationInitialized = false;
	FVector LastAppliedCameraLocation;
	bool bLastAppliedCameraLocationInitialized = false;
	const float SmoothLocationSpeed = 10.0f;

	FEnhancedInputManager EnhancedInputManager;

	FVector EditorMoveAccumulator = FVector::ZeroVector;
	FVector EditorRotateAccumulator = FVector::ZeroVector;
	FVector EditorPanAccumulator = FVector::ZeroVector;
	float EditorZoomAccumulator = 0.0f;
	bool bCameraInputCaptured = false;
	bool bSuppressInputUntilRButtonUp = false;

private:
	void SetupInput();

	void OnMove(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnRotate(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnPan(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnZoom(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);

private:
	FInputMappingContext* EditorMappingContext = nullptr;

	FInputAction* ActionEditorMove = nullptr;
	FInputAction* ActionEditorRotate = nullptr;
	FInputAction* ActionEditorPan = nullptr;
	FInputAction* ActionEditorZoom = nullptr;
};
