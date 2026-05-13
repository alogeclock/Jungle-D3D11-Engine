#include "PreviewViewportClient.h"

#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputRouter.h"
#include "Engine/Input/InputSystem.h"
#include "Input/inputTrigger.h"
#include "Component/CameraComponent.h"
#include "Math/MathUtils.h"
#include "Render/Types/FrameContext.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cmath>

namespace
{
	void DeletePreviewMappingContext(FInputMappingContext* MappingContext);
	bool IsNavigationButtonDown(const FInputSystemSnapshot& Snapshot);
	bool WasNavigationButtonPressed(const FInputSystemSnapshot& Snapshot);
	float GetMaxExtent(const FVector& Extent);
	float GetFocusDistanceForExtent(const FVector& Extent);
	FVector GetDefaultPreviewCameraOffset(float FocusDistance);
}

FPreviewViewportClient::FPreviewViewportClient()
	: FEditorViewportClient(false)
{
	PreviewSettings.LoadFromFile(FPreviewSettings::GetDefaultSettingsPath());
	RenderOptions = PreviewSettings.RenderOptions;
	SetupInput();
}

// 카메라 소유권을 해제하고, 입력 리소스를 정리합니다.
FPreviewViewportClient::~FPreviewViewportClient()
{
	SavePreviewSettings();
	FInputRouter::Get().ClearViewport(this);
	bCameraInputCaptured = false;
	DestroyCamera();
	PreviewScene.Release();
	
	EnhancedInputManager.ClearBindings();
	EnhancedInputManager.ClearAllMappingContexts();
	DeletePreviewMappingContext(PreviewMappingContext);
	PreviewMappingContext = nullptr;

	delete ActionPreviewMove;
	delete ActionPreviewRotate;
	delete ActionPreviewPan;
	delete ActionPreviewZoom;
	delete ActionPreviewOrbit;
	delete ActionPreviewToggleGizmo;
}

// 기존의 EditorRenderPipeline 중앙 제어식 BuildFrame 방식에서 개별 ViewportClient가 FrameContext를 다형성으로 채우도록 개선
void FPreviewViewportClient::SetupFrameContext(FFrameContext& OutFrame, UCameraComponent* InCamera, FViewport* InVP, UWorld* InWorld)
{
	OutFrame.ClearViewportResources();
	OutFrame.SetCameraInfo(InCamera);
	OutFrame.bIsLightView = false;
	OutFrame.SetRenderOptions(GetRenderOptions());
	OutFrame.SetViewportInfo(InVP);

	const FMinimalViewInfo& CameraState = InCamera->GetCameraState();
	const float AR = CameraState.bConstrainAspectRatio
		? CameraState.LetterBoxingAspectW / CameraState.LetterBoxingAspectH
		: CameraState.AspectRatio;
	OutFrame.ApplyConstrainedAR(AR);
	OutFrame.LODContext = InWorld ? InWorld->PrepareLODContext() : FLODUpdateContext();
	OutFrame.CursorViewportX = UINT32_MAX;
	OutFrame.CursorViewportY = UINT32_MAX;
}

void FPreviewViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
	PreviewScene.Initialize();
	CreateCamera();

	if (UWorld* World = PreviewScene.GetWorld())
	{
		World->SetActiveCamera(Camera);
	}
}

// 프리뷰 카메라를 에디터 기본 상태로 복원하고 보간 상태를 초기화한다.
void FPreviewViewportClient::CreateCamera()
{
	FEditorViewportClient::CreateCamera();
	ResetCamera();
}

void FPreviewViewportClient::DestroyCamera()
{
	FEditorViewportClient::DestroyCamera();
	bTargetLocationInitialized = false;
	bLastAppliedCameraLocationInitialized = false;
}

void FPreviewViewportClient::ResetCamera()
{
	if (!Camera)
	{
		return;
	}

	OrbitTarget = PreviewSettings.InitLookAt;
	Camera->SetWorldLocation(PreviewSettings.InitViewPos);
	Camera->LookAt(PreviewSettings.InitLookAt);
	SyncCamera();
	OnCameraReset();
}

// 매 프레임 카메라가 부드럽게 이동하도록 보간 이동시킵니다.
void FPreviewViewportClient::SetViewportRect(float X, float Y, float Width, float Height)
{
	ViewportScreenRect = { X, Y, Width, Height };
	SetViewportSize(Width, Height);
}

void FPreviewViewportClient::Tick(float DeltaTime)
{
	PreviewScene.Tick(DeltaTime);

	if (!Camera)
	{
		return;
	}

	SyncCamera();
	UpdateCameraPosition(DeltaTime);
}

// 뷰포트가 hovered, active, captured 상태일 때만 라우팅된 입력을 수집합니다.
bool FPreviewViewportClient::HandleInputSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	if (!bIsActive && !bIsHovered && !bCameraInputCaptured)
	{
		return false;
	}

	TickInput(Snapshot, DeltaTime);
	return true;
}

void FPreviewViewportClient::SetOrbitTarget(const FVector& InTarget)
{
	OrbitTarget = InTarget;
}

// Preview Object의 크기를 기반으로 적절한 거리로 카메라를 이동합니다.
bool FPreviewViewportClient::FocusBounds(const FVector& Center, const FVector& Extent)
{
	if (!Camera)
	{
		return false;
	}

	OrbitTarget = Center;
	const float FocusDistance = GetFocusDistanceForExtent(Extent);
	const FVector NewLocation = Center + GetDefaultPreviewCameraOffset(FocusDistance);
	Camera->SetWorldLocation(NewLocation);
	Camera->LookAt(Center);
	SyncCamera();
	return true;
}

void FPreviewViewportClient::SetViewportType(ELevelViewportType NewType)
{
	if (!Camera)
	{
		return;
	}

	RenderOptions.ViewportType = NewType;
	MarkPreviewSettingsDirty();
	if (NewType == ELevelViewportType::Perspective)
	{
		Camera->SetOrthographic(false);
		SyncCamera();
		return;
	}
	if (NewType == ELevelViewportType::FreeOrthographic)
	{
		Camera->SetOrthographic(true);
		SyncCamera();
		return;
	}

	Camera->SetOrthographic(true);
	constexpr float OrthoDistance = 50.0f;
	FVector Position = FVector::ZeroVector;
	FVector Rotation = FVector::ZeroVector;
	switch (NewType)
	{
	case ELevelViewportType::Top: Position = FVector(0.0f, 0.0f, OrthoDistance); Rotation = FVector(0.0f, 90.0f, 0.0f); break;
	case ELevelViewportType::Bottom: Position = FVector(0.0f, 0.0f, -OrthoDistance); Rotation = FVector(0.0f, -90.0f, 0.0f); break;
	case ELevelViewportType::Front: Position = FVector(OrthoDistance, 0.0f, 0.0f); Rotation = FVector(0.0f, 0.0f, 180.0f); break;
	case ELevelViewportType::Back: Position = FVector(-OrthoDistance, 0.0f, 0.0f); Rotation = FVector(0.0f, 0.0f, 0.0f); break;
	case ELevelViewportType::Left: Position = FVector(0.0f, -OrthoDistance, 0.0f); Rotation = FVector(0.0f, 0.0f, 90.0f); break;
	case ELevelViewportType::Right: Position = FVector(0.0f, OrthoDistance, 0.0f); Rotation = FVector(0.0f, 0.0f, -90.0f); break;
	default: break;
	}

	const FVector NewLocation = OrbitTarget + Position;
	Camera->SetWorldLocation(NewLocation);
	Camera->SetRelativeRotation(Rotation);
	TargetLocation = NewLocation;
	LastAppliedCameraLocation = NewLocation;
	bTargetLocationInitialized = true;
	bLastAppliedCameraLocationInitialized = true;
}

void FPreviewViewportClient::SetPreviewCameraSpeed(float InSpeed)
{
	PreviewSettings.CameraSpeed = Clamp(InSpeed, 0.1f, 1000.0f);
	MarkPreviewSettingsDirty();
}

void FPreviewViewportClient::TogglePreviewGizmoMode()
{
	SetPreviewGizmoMode((GetPreviewGizmoMode() + 1) % 3);
}

void FPreviewViewportClient::SavePreviewSettings()
{
	if (!bPreviewSettingsDirty)
	{
		return;
	}

	PreviewSettings.RenderOptions = RenderOptions;
	PreviewSettings.SaveToFile(FPreviewSettings::GetDefaultSettingsPath());
	bPreviewSettingsDirty = false;
}

// 프리뷰 카메라용 입력 액션, 매핑 컨텍스트, 콜백 바인딩을 설정한다.
void FPreviewViewportClient::SetupInput()
{
	ActionPreviewMove = new FInputAction("IA_PreviewMove", EInputActionValueType::Axis3D);
	ActionPreviewRotate = new FInputAction("IA_PreviewRotate", EInputActionValueType::Axis2D);
	ActionPreviewPan = new FInputAction("IA_PreviewPan", EInputActionValueType::Axis2D);
	ActionPreviewZoom = new FInputAction("IA_PreviewZoom", EInputActionValueType::Float);
	ActionPreviewOrbit = new FInputAction("IA_PreviewOrbit", EInputActionValueType::Axis2D);
	ActionPreviewToggleGizmo = new FInputAction("IA_PreviewToggleGizmo", EInputActionValueType::Bool);

	PreviewMappingContext = new FInputMappingContext();
	PreviewMappingContext->ContextName = "IMC_EditorPreview";

	PreviewMappingContext->AddMapping(ActionPreviewMove, 'W');
	PreviewMappingContext->AddMapping(ActionPreviewMove, 'S').Modifiers.push_back(new FModifierScale(FVector(-1.0f, 1.0f, 1.0f)));
	PreviewMappingContext->AddMapping(ActionPreviewMove, 'D').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	PreviewMappingContext->AddMapping(ActionPreviewMove, 'A').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	PreviewMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1.0f, -1.0f, 1.0f)));
	PreviewMappingContext->AddMapping(ActionPreviewMove, 'E').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	PreviewMappingContext->AddMapping(ActionPreviewMove, 'Q').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	PreviewMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1.0f, 1.0f, -1.0f)));

	PreviewMappingContext->AddMapping(ActionPreviewRotate, VK_UP).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	PreviewMappingContext->AddMapping(ActionPreviewRotate, VK_DOWN).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	PreviewMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1.0f, -1.0f, 1.0f)));
	PreviewMappingContext->AddMapping(ActionPreviewRotate, VK_LEFT).Modifiers.push_back(new FModifierScale(FVector(-1.0f, 1.0f, 1.0f)));
	PreviewMappingContext->AddMapping(ActionPreviewRotate, VK_RIGHT);
	PreviewMappingContext->AddMapping(ActionPreviewRotate, static_cast<int32>(EInputKey::MouseX));
	PreviewMappingContext->AddMapping(ActionPreviewRotate, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	PreviewMappingContext->AddMapping(ActionPreviewPan, static_cast<int32>(EInputKey::MouseX));
	PreviewMappingContext->AddMapping(ActionPreviewPan, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	PreviewMappingContext->AddMapping(ActionPreviewZoom, static_cast<int32>(EInputKey::MouseWheel));

	PreviewMappingContext->AddMapping(ActionPreviewOrbit, static_cast<int32>(EInputKey::MouseX));
	PreviewMappingContext->AddMapping(ActionPreviewOrbit, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	PreviewMappingContext->AddMapping(ActionPreviewToggleGizmo, VK_SPACE).Triggers.push_back(new FTriggerPressed());

	EnhancedInputManager.AddMappingContext(PreviewMappingContext, 0);
	EnhancedInputManager.BindAction(ActionPreviewMove, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnMove(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionPreviewRotate, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnRotate(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionPreviewPan, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnPan(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionPreviewZoom, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnZoom(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionPreviewOrbit, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnOrbit(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionPreviewToggleGizmo, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnToggleGizmoMode(V, Snapshot); });
}

// 라우팅된 입력 스냅샷을 기반으로 카메라 변화량을 누적하고, 이번 프레임의 이동을 적용합니다.
void FPreviewViewportClient::TickInput(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	if (!Camera)
	{
		return;
	}

	MoveDelta = FVector::ZeroVector;
	RotateDelta = FVector::ZeroVector;
	PanDelta = FVector::ZeroVector;
	OrbitDelta = FVector::ZeroVector;
	ZoomDelta = 0.0f;

	const bool bGuiWantsMouse = Snapshot.IsGuiUsingMouse();
	const bool bGuiWantsKeyboard = Snapshot.IsGuiUsingKeyboard();

	if (!IsNavigationButtonDown(Snapshot) || !Snapshot.IsWindowFocused())
	{
		bSuppressInputUntilMouseUp = false;
	}

	if (bIsHovered && WasNavigationButtonPressed(Snapshot) && !bGuiWantsMouse && !bGuiWantsKeyboard)
	{
		bCameraInputCaptured = true;
		bSuppressInputUntilMouseUp = false;
		FInputRouter::Get().SetMouseCapturedViewport(this);
	}
	else if (IsNavigationButtonDown(Snapshot) && !bCameraInputCaptured && (bIsHovered || bIsActive))
	{
		bSuppressInputUntilMouseUp = true;
	}

	if (!IsNavigationButtonDown(Snapshot) || !Snapshot.IsWindowFocused())
	{
		bCameraInputCaptured = false;
		FInputRouter::Get().ReleaseMouseCapture(this);
	}

	const bool bAllowNewViewportInput = !bSuppressInputUntilMouseUp && (bIsHovered || bIsActive) && !bGuiWantsMouse && !bGuiWantsKeyboard;
	const bool bIgnoreGui = !bSuppressInputUntilMouseUp && (bCameraInputCaptured || bAllowNewViewportInput);

	if (bSuppressInputUntilMouseUp)
	{
		return;
	}

	EnhancedInputManager.ProcessInput(Snapshot, DeltaTime, bIgnoreGui);

	const float MoveSensitivity = RenderOptions.CameraMoveSensitivity;
	const float RotateSensitivity = RenderOptions.CameraRotateSensitivity;
	const float CameraSpeed = PreviewSettings.CameraSpeed * MoveSensitivity;

	const FMinimalViewInfo& CameraState = Camera->GetCameraState();
	const bool              bIsOrtho    = CameraState.bIsOrthogonal;

	if (!bIsOrtho)
	{
		const float PanMouseScale = CameraSpeed * 0.01f;

		if (!MoveDelta.IsNearlyZero())
		{
			FVector DeltaMove = (Camera->GetForwardVector() * MoveDelta.X + Camera->GetRightVector() * MoveDelta.Y) * (CameraSpeed * DeltaTime);
			DeltaMove.Z       += MoveDelta.Z * (CameraSpeed * DeltaTime);

			TargetLocation = TargetLocation + DeltaMove;
			OrbitTarget    = OrbitTarget + DeltaMove;
		}

		if (!PanDelta.IsNearlyZero())
		{
			FVector DeltaPan = Camera->GetRightVector() * (-PanDelta.X * PanMouseScale * 0.15f) + Camera->GetUpVector() * (-PanDelta.Y * PanMouseScale * 0.15f);
			TargetLocation   = TargetLocation + DeltaPan;
			OrbitTarget      = OrbitTarget + DeltaPan;
		}

		if (!RotateDelta.IsNearlyZero())
		{
			const float MouseRotationSpeed = 0.15f * RotateSensitivity;
			const float AngleVelocity      = PreviewSettings.CameraRotationSpeed * RotateSensitivity;

			float Yaw   = 0.0f;
			float Pitch = 0.0f;

			if (!bCameraInputCaptured || !Snapshot.IsMouseButtonDown(VK_RBUTTON))
			{
				Yaw   = RotateDelta.X * AngleVelocity * DeltaTime;
				Pitch = RotateDelta.Y * AngleVelocity * DeltaTime;
			}
			else
			{
				Yaw   = RotateDelta.X * MouseRotationSpeed;
				Pitch = RotateDelta.Y * MouseRotationSpeed;
			}
			Camera->Rotate(Yaw, Pitch);
		}

		ApplyOrbitInput();

		const float ZoomSpeed = PreviewSettings.CameraZoomSpeed;
		if (std::abs(ZoomDelta) > 1e-6f)
		{
			TargetLocation = TargetLocation + Camera->GetForwardVector() * (ZoomSpeed * ZoomDelta * 0.015f);
		}

		return;
	}

	{
		const float VPWidth  = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
		const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;

		const float SafeVPWidth  = (std::max)(VPWidth, 1.0f);
		const float SafeVPHeight = (std::max)(VPHeight, 1.0f);
		const float SafeAspect   = (std::max)(CameraState.AspectRatio, 1.0e-6f);

		const float OrthoWidth  = (std::max)(CameraState.OrthoWidth, 0.1f);
		const float OrthoHeight = OrthoWidth / SafeAspect;

		const float WorldPerPixelX = OrthoWidth / SafeVPWidth;
		const float WorldPerPixelY = OrthoHeight / SafeVPHeight;

		auto ApplyOrthoPan = [&](const FVector& MouseDelta)
		{
			const FVector DeltaPan =
				Camera->GetRightVector() * (-MouseDelta.X * WorldPerPixelX * MoveSensitivity) +
				Camera->GetUpVector() * (MouseDelta.Y * WorldPerPixelY * MoveSensitivity);

			TargetLocation = TargetLocation + DeltaPan;
			OrbitTarget    = OrbitTarget + DeltaPan;
		};

		// MMB drag: orthographic pan must use mouse pan delta, not keyboard move delta.
		if (!PanDelta.IsNearlyZero())
		{
			ApplyOrthoPan(PanDelta);
		}

		// RMB drag: fixed orthographic views should pan instead of rotating the camera.
		if (!RotateDelta.IsNearlyZero() && bCameraInputCaptured && Snapshot.IsMouseButtonDown(VK_RBUTTON))
		{
			ApplyOrthoPan(RotateDelta);
		}

		// Alt + LMB: allow orbit only in FreeOrthographic; in fixed ortho views use it as pan.
		if (!OrbitDelta.IsNearlyZero() && bCameraInputCaptured && Snapshot.IsKeyDown(VK_MENU) && Snapshot.IsMouseButtonDown(VK_LBUTTON))
		{
			if (RenderOptions.ViewportType == ELevelViewportType::FreeOrthographic)
			{
				ApplyOrbitInput();
			}
			else
			{
				ApplyOrthoPan(OrbitDelta);
			}
		}

		if (std::abs(ZoomDelta) > 1e-6f)
		{
			const float ZoomFactor    = std::pow(0.9f, ZoomDelta);
			const float NewOrthoWidth = std::clamp(OrthoWidth * ZoomFactor, 0.1f, 100000.0f);
			Camera->SetOrthoWidth(NewOrthoWidth);
		}
	}
}

// 마우스 오른쪽 버튼으로 카메라를 제어하고 있을 때, 자유 비행 카메라 이동량을 누적합니다.
void FPreviewViewportClient::OnMove(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (Snapshot.IsKeyDown(VK_CONTROL))
	{
		return;
	}

	if (bCameraInputCaptured && Snapshot.IsMouseButtonDown(VK_RBUTTON))
	{
		MoveDelta = MoveDelta + Value.GetVector();
	}
}

// 카메라 내비게이션 중 마우스, 키보드에 의한 회전량을 누적합니다.
void FPreviewViewportClient::OnRotate(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (Snapshot.IsMouseButtonDown(VK_RBUTTON))
	{
		if (!bCameraInputCaptured)
		{
			return;
		}
		RotateDelta = RotateDelta + Value.GetVector();
	}
	else
	{
		if (!bCameraInputCaptured)
		{
			return;
		}

		FVector KeyboardRotate(0.0f, 0.0f, 0.0f);
		if (Snapshot.IsKeyDown(VK_RIGHT)) KeyboardRotate.X += 1.0f;
		if (Snapshot.IsKeyDown(VK_LEFT)) KeyboardRotate.X -= 1.0f;
		if (Snapshot.IsKeyDown(VK_UP)) KeyboardRotate.Y += 1.0f;
		if (Snapshot.IsKeyDown(VK_DOWN)) KeyboardRotate.Y -= 1.0f;

		if (!KeyboardRotate.IsNearlyZero())
		{
			RotateDelta = RotateDelta + KeyboardRotate;
		}
	}
}

// 마우스 휠 버튼을 이용한 카메라의 상하좌우 팬 이동량을 누적합니다.
void FPreviewViewportClient::OnPan(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (bCameraInputCaptured && Snapshot.IsMouseButtonDown(VK_MBUTTON))
	{
		PanDelta = PanDelta + Value.GetVector();
	}
}


// 마우스 휠을 통한 줌 또는 오른쪽 버튼 캡처 중 카메라 이동 속도를 조절합니다.
void FPreviewViewportClient::OnZoom(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (!bIsHovered)
	{
		return;
	}

	if (bCameraInputCaptured && Snapshot.IsMouseButtonDown(VK_RBUTTON))
	{
		SetPreviewCameraSpeed(Clamp(PreviewSettings.CameraSpeed + Value.Get() * 2.0f, 1.0f, 100.0f));
		return;
	}

	ZoomDelta += Value.Get();
}

// Alt-drag를 통한 궤도 회전 변화량을 누적합니다.
void FPreviewViewportClient::OnOrbit(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (!Snapshot.IsKeyDown(VK_MENU))
	{
		return;
	}

	if (bCameraInputCaptured && Snapshot.IsMouseButtonDown(VK_LBUTTON))
	{
		OrbitDelta = OrbitDelta + Value.GetVector();
	}
}

// 외부에서 카메라가 강제로 이동된 경우, 보간 목표 지점을 동기화합니다.
void FPreviewViewportClient::OnToggleGizmoMode(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	(void)Value;
	if (Snapshot.IsGuiUsingKeyboard())
	{
		return;
	}

	TogglePreviewGizmoMode();
}

void FPreviewViewportClient::SyncCamera()
{
	if (!Camera)
	{
		bTargetLocationInitialized = false;
		bLastAppliedCameraLocationInitialized = false;
		return;
	}

	const FVector CurrentLocation = Camera->GetWorldLocation();
	const bool bCameraMovedExternally =
		bLastAppliedCameraLocationInitialized &&
		FVector::Distance(CurrentLocation, LastAppliedCameraLocation) > 0.01f;

	if (!bTargetLocationInitialized || bCameraMovedExternally)
	{
		TargetLocation = CurrentLocation;
		bTargetLocationInitialized = true;
	}

	LastAppliedCameraLocation = CurrentLocation;
	bLastAppliedCameraLocationInitialized = true;
}

// 현재 타겟 위치에 따라 카메라를 보간한다.
void FPreviewViewportClient::UpdateCameraPosition(float DeltaTime)
{
	if (!Camera)
	{
		return;
	}

	const FVector CurrentLocation = Camera->GetWorldLocation();
	const float LerpAlpha = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
	const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
	Camera->SetWorldLocation(NewLocation);
	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
}

// 누적된 Orbit 회전 입력을 적용하여, Orbit Target과의 거리를 유지합니다.
void FPreviewViewportClient::ApplyOrbitInput()
{
	if (!Camera || OrbitDelta.IsNearlyZero())
	{
		return;
	}

	const FMinimalViewInfo& CameraState = Camera->GetCameraState();
	if (CameraState.bIsOrthogonal && RenderOptions.ViewportType != ELevelViewportType::FreeOrthographic)
	{
		OrbitDelta = FVector::ZeroVector;
		return;
	}

	const float Sensitivity = 0.25f * RenderOptions.CameraRotateSensitivity;
	FRotator Rotation = Camera->GetRelativeRotation();
	Rotation.Yaw += OrbitDelta.X * Sensitivity;
	Rotation.Pitch = Clamp(Rotation.Pitch + OrbitDelta.Y * Sensitivity, -89.0f, 89.0f);

	const FVector CurrentLocation = Camera->GetWorldLocation();
	float Distance = FVector::Distance(CurrentLocation, OrbitTarget);
	if (Distance < 0.1f)
	{
		Distance = GetFocusDistanceForExtent(FVector(1.0f, 1.0f, 1.0f));
	}

	const FVector NewLocation = OrbitTarget - Rotation.GetForwardVector() * Distance;
	Camera->SetWorldLocation(NewLocation);
	Camera->SetRelativeRotation(Rotation);
	TargetLocation = NewLocation;
	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
	bTargetLocationInitialized = true;
}

namespace
{
	// Trigger, Modifier를 소거하여 Preview Mapping Context를 삭제합니다.
	void DeletePreviewMappingContext(FInputMappingContext* MappingContext)
	{
		if (!MappingContext)
		{
			return;
		}

		for (FActionKeyMapping& Mapping : MappingContext->Mappings)
		{
			for (FInputTrigger* Trigger : Mapping.Triggers)
			{
				delete Trigger;
			}
			for (FInputModifier* Modifier : Mapping.Modifiers)
			{
				delete Modifier;
			}
		}

		delete MappingContext;
	}

	// Navigation Button(우클릭, 휠 등)의 상태를 확인합니다.
	bool IsNavigationButtonDown(const FInputSystemSnapshot& Snapshot)
	{
		return Snapshot.IsMouseButtonDown(VK_RBUTTON) ||
			Snapshot.IsMouseButtonDown(VK_MBUTTON) ||
			(Snapshot.IsKeyDown(VK_MENU) && Snapshot.IsMouseButtonDown(VK_LBUTTON));
	}

	// Navigation Button(우클릭, 휠 등)의 이전 상태를 확인합니다.
	bool WasNavigationButtonPressed(const FInputSystemSnapshot& Snapshot)
	{
		return Snapshot.IsMouseButtonPressed(VK_RBUTTON) ||
			Snapshot.IsMouseButtonPressed(VK_MBUTTON) ||
			(Snapshot.IsKeyDown(VK_MENU) && Snapshot.IsMouseButtonPressed(VK_LBUTTON));
	}

	float GetMaxExtent(const FVector& Extent)
	{
		return (std::max)({ std::abs(Extent.X), std::abs(Extent.Y), std::abs(Extent.Z) });
	}

	float GetFocusDistanceForExtent(const FVector& Extent)
	{
		const float Radius = (std::max)(GetMaxExtent(Extent), 1.0f);
		return (std::max)(5.0f, Radius * 2.5f);
	}

	FVector GetDefaultPreviewCameraOffset(float FocusDistance)
	{
		const FVector Direction = FVector(0.5f, -1.0f, 1.0f).Normalized();
		return Direction * FocusDistance;
	}
}
