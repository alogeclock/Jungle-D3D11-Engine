#include "PreviewViewportClient.h"

#include "Editor/Settings/EditorSettings.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputRouter.h"
#include "Engine/Input/InputSystem.h"
#include "Component/CameraComponent.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"
#include "Viewport/Viewport.h"

#include <algorithm>

namespace
{
	void DeletePreviewMappingContext(FInputMappingContext* MappingContext);
	bool IsNavigationButtonDown(const FInputSystemSnapshot& Snapshot);
	bool WasNavigationButtonPressed(const FInputSystemSnapshot& Snapshot);
	float GetMaxExtent(const FVector& Extent);
	float GetFocusDistanceForExtent(const FVector& Extent);
	FVector GetDefaultCameraLocation();
}

FPreviewViewportClient::FPreviewViewportClient()
{
	SetupInput();
}

// 카메라 소유권을 해제하고, 입력 리소스를 정리합니다.
FPreviewViewportClient::~FPreviewViewportClient()
{
	FInputRouter::Get().ClearViewport(this);
	bCameraInputCaptured = false;
	DestroyCamera();
	
	EnhancedInputManager.ClearBindings();
	EnhancedInputManager.ClearAllMappingContexts();
	DeletePreviewMappingContext(PreviewMappingContext);
	PreviewMappingContext = nullptr;

	delete ActionPreviewMove;
	delete ActionPreviewRotate;
	delete ActionPreviewPan;
	delete ActionPreviewZoom;
	delete ActionPreviewOrbit;
}

void FPreviewViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
}

void FPreviewViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
	ResetCamera();
}

void FPreviewViewportClient::DestroyCamera()
{
	if (Camera)
	{
		UObjectManager::Get().DestroyObject(Camera);
		Camera = nullptr;
	}

	bTargetLocationInitialized = false;
	bLastAppliedCameraLocationInitialized = false;
}

// 프리뷰 카메라를 에디터 기본 상태로 복원하고 보간 상태를 초기화한다.
void FPreviewViewportClient::ResetCamera()
{
	if (!Camera)
	{
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();
	OrbitTarget = Settings.InitLookAt;
	Camera->SetWorldLocation(Settings.InitViewPos);
	Camera->LookAt(Settings.InitLookAt);
	PreviewCameraSpeed = Settings.CameraSpeed;
	SyncCamera();
	OnCameraReset();
}

// 매 프레임 카메라가 부드럽게 이동하도록 보간 이동시킵니다.
void FPreviewViewportClient::Tick(float DeltaTime)
{
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

// 프리뷰 뷰포트의 활성화 상태에 따라 Keyboard 라우팅 상태를 업데이트합니다.
void FPreviewViewportClient::SetActive(bool bInActive)
{
	bIsActive = bInActive;
	if (bIsActive)
	{
		FInputRouter::Get().SetKeyboardFocusedViewport(this);
	}
	else if (FInputRouter::Get().GetKeyboardFocusedViewport() == this)
	{
		FInputRouter::Get().SetKeyboardFocusedViewport(nullptr);
	}
}

// InputRouter가 마우스 호버 상태에 따라 뷰포트를 타게팅할 수 있도록 업데이트합니다.
void FPreviewViewportClient::SetHovered(bool bInHovered)
{
	bIsHovered = bInHovered;
	if (bIsHovered)
	{
		FInputRouter::Get().SetHoveredViewport(this);
	}
	else if (FInputRouter::Get().GetHoveredViewport() == this)
	{
		FInputRouter::Get().SetHoveredViewport(nullptr);
	}
}

void FPreviewViewportClient::SetViewportRect(float X, float Y, float Width, float Height)
{
	ViewportScreenRect = { X, Y, Width, Height };

	if (Camera)
	{
		Camera->OnResize(static_cast<int32>(Width), static_cast<int32>(Height));
	}
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

	FVector Forward = Camera->GetForwardVector();
	if (Forward.IsNearlyZero())
	{
		Forward = (Center - GetDefaultCameraLocation());
		if (Forward.IsNearlyZero())
		{
			Forward = FVector(1.0f, 0.0f, 0.0f);
		}
		Forward.Normalize();
	}

	const FVector NewLocation = Center - Forward * FocusDistance;
	Camera->SetWorldLocation(NewLocation);
	Camera->LookAt(Center);
	SyncCamera();
	return true;
}

// 프리뷰 카메라용 입력 액션, 매핑 컨텍스트, 콜백 바인딩을 설정한다.
void FPreviewViewportClient::SetupInput()
{
	ActionPreviewMove = new FInputAction("IA_PreviewMove", EInputActionValueType::Axis3D);
	ActionPreviewRotate = new FInputAction("IA_PreviewRotate", EInputActionValueType::Axis2D);
	ActionPreviewPan = new FInputAction("IA_PreviewPan", EInputActionValueType::Axis2D);
	ActionPreviewZoom = new FInputAction("IA_PreviewZoom", EInputActionValueType::Float);
	ActionPreviewOrbit = new FInputAction("IA_PreviewOrbit", EInputActionValueType::Axis2D);

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

	EnhancedInputManager.AddMappingContext(PreviewMappingContext, 0);
	EnhancedInputManager.BindAction(ActionPreviewMove, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnMove(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionPreviewRotate, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnRotate(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionPreviewPan, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnPan(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionPreviewZoom, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnZoom(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionPreviewOrbit, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnOrbit(V, Snapshot); });
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
	const float CameraSpeed = PreviewCameraSpeed * MoveSensitivity;
	const float PanMouseScale = CameraSpeed * 0.01f;

	if (!MoveDelta.IsNearlyZero())
	{
		FVector DeltaMove = (Camera->GetForwardVector() * MoveDelta.X + Camera->GetRightVector() * MoveDelta.Y) * (CameraSpeed * DeltaTime);
		DeltaMove.Z += MoveDelta.Z * (CameraSpeed * DeltaTime);
		TargetLocation = TargetLocation + DeltaMove;
		OrbitTarget = OrbitTarget + DeltaMove;
	}

	if (!PanDelta.IsNearlyZero())
	{
		FVector DeltaPan = (Camera->GetRightVector() * (-PanDelta.X * PanMouseScale * 0.15f)) + (Camera->GetUpVector() * (PanDelta.Y * PanMouseScale * 0.15f));
		TargetLocation = TargetLocation + DeltaPan;
		OrbitTarget = OrbitTarget + DeltaPan;
	}

	if (!RotateDelta.IsNearlyZero())
	{
		const float MouseRotationSpeed = 0.15f * RotateSensitivity;
		const float AngleVelocity = FEditorSettings::Get().CameraRotationSpeed * RotateSensitivity;
		float Yaw;
		float Pitch;

		if (!bCameraInputCaptured || !Snapshot.IsMouseButtonDown(VK_RBUTTON))
		{
			Yaw = RotateDelta.X * AngleVelocity * DeltaTime;
			Pitch = RotateDelta.Y * AngleVelocity * DeltaTime;
		}
		else
		{
			Yaw = RotateDelta.X * MouseRotationSpeed;
			Pitch = RotateDelta.Y * MouseRotationSpeed;
		}

		Camera->Rotate(Yaw, Pitch);
	}

	ApplyOrbitInput();

	const float ZoomSpeed = FEditorSettings::Get().CameraZoomSpeed;
	if (std::abs(ZoomDelta) > 1e-6f)
	{
		TargetLocation = TargetLocation + Camera->GetForwardVector() * (ZoomDelta * ZoomSpeed * 0.015f);
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
	if (bCameraInputCaptured && Snapshot.IsMouseButtonDown(VK_RBUTTON))
	{
		PreviewCameraSpeed = Clamp(PreviewCameraSpeed + Value.Get() * 2.0f, 1.0f, 100.0f);
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

	const FVector NewLocation = OrbitTarget - Rotation.ToVector() * Distance;
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

	FVector GetDefaultCameraLocation()
	{
		return FVector(10.0f, 0.0f, 5.0f);
	}
}
