#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/Settings/EditorSettings.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputRouter.h"
#include "Component/CameraComponent.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"
#include "Viewport/Viewport.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cmath>

namespace
{
	void DeleteMappingContext(FInputMappingContext* MappingContext);
	bool TryConvertMouseToPixel(const ImVec2& Pos, const FRect& Rect, const FViewport* Viewport, float FallbackW, float FallbackH, float& OutX, float& OutY);
}

FEditorViewportClient::FEditorViewportClient()
{
	SetupInput();
}

FEditorViewportClient::~FEditorViewportClient()
{
	EnhancedInputManager.ClearBindings();
	EnhancedInputManager.ClearAllMappingContexts();

	DeleteMappingContext(EditorMappingContext);
	EditorMappingContext = nullptr;

	delete ActionEditorMove;
	delete ActionEditorRotate;
	delete ActionEditorPan;
	delete ActionEditorZoom;
}

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
}

void FEditorViewportClient::SetupInput()
{
	ActionEditorMove = new FInputAction("IA_EditorMove", EInputActionValueType::Axis3D);
	ActionEditorRotate = new FInputAction("IA_EditorRotate", EInputActionValueType::Axis2D);
	ActionEditorPan = new FInputAction("IA_EditorPan", EInputActionValueType::Axis2D);
	ActionEditorZoom = new FInputAction("IA_EditorZoom", EInputActionValueType::Float);

	EditorMappingContext = new FInputMappingContext();
	EditorMappingContext->ContextName = "IMC_EditorViewport";

	EditorMappingContext->AddMapping(ActionEditorMove, 'W');
	EditorMappingContext->AddMapping(ActionEditorMove, 'S').Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
	EditorMappingContext->AddMapping(ActionEditorMove, 'D').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->AddMapping(ActionEditorMove, 'A').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
	EditorMappingContext->AddMapping(ActionEditorMove, 'E').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	EditorMappingContext->AddMapping(ActionEditorMove, 'Q').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, 1, -1)));

	EditorMappingContext->AddMapping(ActionEditorRotate, VK_UP).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_DOWN).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_LEFT).Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_RIGHT);

	EditorMappingContext->AddMapping(ActionEditorRotate, static_cast<int32>(EInputKey::MouseX));
	EditorMappingContext->AddMapping(ActionEditorRotate, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	EditorMappingContext->AddMapping(ActionEditorPan, static_cast<int32>(EInputKey::MouseX));
	EditorMappingContext->AddMapping(ActionEditorPan, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	EditorMappingContext->AddMapping(ActionEditorZoom, static_cast<int32>(EInputKey::MouseWheel));

	EnhancedInputManager.AddMappingContext(EditorMappingContext, 0);
	EnhancedInputManager.BindAction(ActionEditorMove, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnMove(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorRotate, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnRotate(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorPan, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnPan(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorZoom, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnZoom(V, Snapshot); });
}

void FEditorViewportClient::OnMove(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (Snapshot.IsKeyDown(VK_CONTROL))
	{
		return;
	}

	if (bCameraInputCaptured && Snapshot.IsMouseButtonDown(VK_RBUTTON))
	{
		EditorMoveAccumulator = EditorMoveAccumulator + Value.GetVector();
	}
}

void FEditorViewportClient::OnRotate(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (Snapshot.IsMouseButtonDown(VK_RBUTTON))
	{
		if (!bCameraInputCaptured)
		{
			return;
		}
		EditorRotateAccumulator = EditorRotateAccumulator + Value.GetVector();
	}
	else
	{
		if (!bCameraInputCaptured)
		{
			return;
		}

		FVector KeyboardRotate(0, 0, 0);
		if (Snapshot.IsKeyDown(VK_RIGHT)) KeyboardRotate.X += 1.0f;
		if (Snapshot.IsKeyDown(VK_LEFT)) KeyboardRotate.X -= 1.0f;
		if (Snapshot.IsKeyDown(VK_UP)) KeyboardRotate.Y += 1.0f;
		if (Snapshot.IsKeyDown(VK_DOWN)) KeyboardRotate.Y -= 1.0f;
		if (!KeyboardRotate.IsNearlyZero())
		{
			EditorRotateAccumulator = EditorRotateAccumulator + KeyboardRotate;
		}
	}
}

void FEditorViewportClient::OnPan(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (Snapshot.IsMouseButtonDown(VK_MBUTTON))
	{
		EditorPanAccumulator = EditorPanAccumulator + Value.GetVector();
	}
	else if (Snapshot.IsKeyDown(VK_MENU) && Snapshot.IsMouseButtonDown(VK_MBUTTON))
	{
		EditorPanAccumulator = EditorPanAccumulator + Value.GetVector();
	}
}

void FEditorViewportClient::OnZoom(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (Snapshot.IsMouseButtonDown(VK_RBUTTON))
	{
		if (!bCameraInputCaptured)
		{
			return;
		}

		float& Speed = FEditorSettings::Get().CameraSpeed;
		Speed = Clamp(Speed + Value.Get() * 2.0f, 1.0f, 100.0f);
		return;
	}

	EditorZoomAccumulator += Value.Get();
}

void FEditorViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
}

void FEditorViewportClient::DestroyCamera()
{
	if (Camera)
	{
		UObjectManager::Get().DestroyObject(Camera);
		Camera = nullptr;
	}
}

void FEditorViewportClient::ResetCamera()
{
	if (!Camera || !Settings)
	{
		return;
	}

	Camera->SetWorldLocation(Settings->InitViewPos);
	Camera->LookAt(Settings->InitLookAt);
	SyncCameraSmoothingTarget();
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	if (InWidth > 0.0f)
	{
		WindowWidth = InWidth;
	}
	if (InHeight > 0.0f)
	{
		WindowHeight = InHeight;
	}
	if (Camera)
	{
		Camera->OnResize(static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
	}
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (!bIsActive)
	{
		return;
	}

	SyncCameraSmoothingTarget();
	if (bIsFocusAnimating && Camera)
	{
		FocusAnimTimer += DeltaTime;
		float Alpha = FocusAnimTimer / FocusAnimDuration;
		if (Alpha >= 1.0f)
		{
			Alpha = 1.0f;
			bIsFocusAnimating = false;
		}

		const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
		const FVector NewLoc = FocusStartLoc * (1.0f - SmoothAlpha) + FocusEndLoc * SmoothAlpha;
		const FQuat StartQuat = FocusStartRot.ToQuaternion();
		const FQuat EndQuat = FocusEndRot.ToQuaternion();
		const FQuat BlendedQuat = FQuat::Slerp(StartQuat, EndQuat, SmoothAlpha);
		Camera->SetWorldLocation(NewLoc);
		Camera->SetRelativeRotation(FRotator::FromQuaternion(BlendedQuat));
		TargetLocation = NewLoc;
		LastAppliedCameraLocation = NewLoc;
		bLastAppliedCameraLocationInitialized = true;
	}
	else
	{
		ApplySmoothedCameraLocation(DeltaTime);
	}
}

bool FEditorViewportClient::HandleInputSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	if (!bIsActive && !bIsHovered && !bCameraInputCaptured)
	{
		return false;
	}

	TickInput(Snapshot, DeltaTime);
	return true;
}

void FEditorViewportClient::SyncCameraSmoothingTarget()
{
	if (!Camera)
	{
		bTargetLocationInitialized = false;
		bLastAppliedCameraLocationInitialized = false;
		return;
	}

	const FVector CurrentLocation = Camera->GetWorldLocation();
	const bool bCameraMovedExternally = bLastAppliedCameraLocationInitialized && FVector::DistSquared(CurrentLocation, LastAppliedCameraLocation) > 0.0001f;
	if (!bTargetLocationInitialized || bCameraMovedExternally)
	{
		TargetLocation = CurrentLocation;
		bTargetLocationInitialized = true;
	}
	LastAppliedCameraLocation = CurrentLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
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

void FEditorViewportClient::TickInput(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	if (!Camera || !CanProcessCameraInput())
	{
		return;
	}

	EditorMoveAccumulator = FVector::ZeroVector;
	EditorRotateAccumulator = FVector::ZeroVector;
	EditorPanAccumulator = FVector::ZeroVector;
	EditorZoomAccumulator = 0.0f;

	const bool bGuiWantsMouse = Snapshot.IsGuiUsingMouse();
	const bool bGuiWantsKeyboard = Snapshot.IsGuiUsingKeyboard();

	if (!Snapshot.IsMouseButtonDown(VK_RBUTTON) || !Snapshot.IsWindowFocused())
	{
		bSuppressInputUntilRButtonUp = false;
	}

	if (bIsHovered && Snapshot.IsMouseButtonPressed(VK_RBUTTON) && !bGuiWantsMouse && !bGuiWantsKeyboard)
	{
		bCameraInputCaptured = true;
		bSuppressInputUntilRButtonUp = false;
		FInputRouter::Get().SetMouseCapturedViewport(this);
	}
	else if (Snapshot.IsMouseButtonDown(VK_RBUTTON) && !bCameraInputCaptured && (bIsHovered || bIsActive))
	{
		bSuppressInputUntilRButtonUp = true;
	}

	if ((Snapshot.IsMouseButtonReleased(VK_RBUTTON) && !Snapshot.IsMouseButtonDown(FInputManager::MOUSE_LEFT)) || !Snapshot.IsWindowFocused())
	{
		bCameraInputCaptured = false;
		FInputRouter::Get().ReleaseMouseCapture(this);
	}

	const bool bAllowNewViewportInput = !bSuppressInputUntilRButtonUp && (bIsHovered || bIsActive) && !bGuiWantsMouse && !bGuiWantsKeyboard;
	const bool bIgnoreGui = !bSuppressInputUntilRButtonUp && (bCameraInputCaptured || bAllowNewViewportInput);

	if (bSuppressInputUntilRButtonUp)
	{
		return;
	}

	EnhancedInputManager.ProcessInput(Snapshot, DeltaTime, bIgnoreGui);

	const FMinimalViewInfo& CameraState = Camera->GetCameraState();
	const bool bIsOrtho = CameraState.bIsOrthogonal;
	const float MoveSensitivity = RenderOptions.CameraMoveSensitivity;
	const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;
	const float PanMouseScale = CameraSpeed * 0.01f;
	if (!bIsOrtho)
	{
		const FVector DeltaMove = (Camera->GetForwardVector() * EditorMoveAccumulator.X + Camera->GetRightVector() * EditorMoveAccumulator.Y) * (CameraSpeed * DeltaTime)
			+ FVector(0.0f, 0.0f, EditorMoveAccumulator.Z * (CameraSpeed * DeltaTime));
		TargetLocation += DeltaMove;

		if (!EditorPanAccumulator.IsNearlyZero())
		{
			const FVector PanDelta = (Camera->GetRightVector() * (-EditorPanAccumulator.X * PanMouseScale * 0.15f)) + (Camera->GetUpVector() * (EditorPanAccumulator.Y * PanMouseScale * 0.15f));
			TargetLocation += PanDelta;
		}

		if (!EditorRotateAccumulator.IsNearlyZero())
		{
			const float RotateSensitivity = RenderOptions.CameraRotateSensitivity;
			const float MouseRotationSpeed = 0.15f * RotateSensitivity;
			const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
			float Yaw = 0.0f;
			float Pitch = 0.0f;
			if (!bCameraInputCaptured || !Snapshot.IsMouseButtonDown(VK_RBUTTON))
			{
				Yaw = EditorRotateAccumulator.X * AngleVelocity * DeltaTime;
				Pitch = EditorRotateAccumulator.Y * AngleVelocity * DeltaTime;
			}
			else
			{
				Yaw = EditorRotateAccumulator.X * MouseRotationSpeed;
				Pitch = EditorRotateAccumulator.Y * MouseRotationSpeed;
			}
			Camera->Rotate(Yaw, Pitch);
		}
	}
	else if (!EditorRotateAccumulator.IsNearlyZero() && bCameraInputCaptured && Snapshot.IsMouseButtonDown(VK_RBUTTON))
	{
		const float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;
		Camera->MoveLocal(FVector(0, -EditorRotateAccumulator.Y * PanScale, EditorRotateAccumulator.Z * PanScale));
	}

	const float ZoomSpeed = Settings ? Settings->CameraZoomSpeed : 300.f;
	if (std::abs(EditorZoomAccumulator) > 1e-6f)
	{
		if (Camera->IsOrthogonal())
		{
			const float NewWidth = Camera->GetOrthoWidth() - EditorZoomAccumulator * ZoomSpeed * DeltaTime;
			Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f));
		}
		else
		{
			TargetLocation += Camera->GetForwardVector() * (EditorZoomAccumulator * ZoomSpeed * 0.015f);
		}
	}
}

bool FEditorViewportClient::GetCursorViewportPosition(uint32& OutX, uint32& OutY) const
{
	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	if (!bIsHovered && !bIsActive)
	{
		return false;
	}

	float ViewportX = 0.0f;
	float ViewportY = 0.0f;
	if (TryConvertMouseToPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, ViewportX, ViewportY))
	{
		OutX = static_cast<uint32>(ViewportX);
		OutY = static_cast<uint32>(ViewportY);
		return true;
	}

	return false;
}

namespace
{
	void DeleteMappingContext(FInputMappingContext* MappingContext)
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

	bool TryConvertMouseToPixel(const ImVec2& Pos, const FRect& Rect, const FViewport* Viewport, float FallbackW, float FallbackH, float& OutX, float& OutY)
	{
		if (Rect.Width <= 0.0f || Rect.Height <= 0.0f)
		{
			return false;
		}

		const float LocalX = Pos.x - Rect.X;
		const float LocalY = Pos.y - Rect.Y;
		if (LocalX < 0.0f || LocalY < 0.0f || LocalX >= Rect.Width || LocalY >= Rect.Height)
		{
			return false;
		}

		const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : FallbackW;
		const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : FallbackH;
		if (TargetWidth <= 0.0f || TargetHeight <= 0.0f)
		{
			return false;
		}

		const float ScaleX = TargetWidth / Rect.Width;
		const float ScaleY = TargetHeight / Rect.Height;
		OutX = LocalX * ScaleX;
		OutY = LocalY * ScaleY;
		return true;
	}
}
