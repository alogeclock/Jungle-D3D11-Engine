#include "Editor/Viewport/LevelEditorViewportClient.h"

#include "Core/ProjectSettings.h"
#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputRouter.h"
#include "Engine/Input/InputTrigger.h"
#include "Engine/Profiling/PlatformTime.h"
#include "Engine/Runtime/Engine.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"

#include "Collision/RayUtils.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/MeshComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/UIImageComponent.h"
#include "Component/UIScreenTextComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <type_traits>

namespace
{
	struct FCameraBookmark
	{
		FVector Location;
		FRotator Rotation;
		bool bValid = false;
	};

	enum class EUIScreenGizmoAxis : int32
	{
		None = 0,
		X = 1,
		Y = 2,
		XY = 3
	};

	FCameraBookmark GCameraBookmarks[10];

	FInputAction* CreateLevelInputAction(TArray<FInputAction*>& Actions, const char* Name, EInputActionValueType ValueType);
	void DeleteMappingContext(FInputMappingContext* MappingContext);
	void DuplicateSelectedActors(FSelectionManager* SelectionManager, UGizmoComponent* Gizmo, const FVector& OffsetStep = FVector::ZeroVector);
	void SyncPIEViewportRect(FViewport* Viewport, const FRect& ViewportScreenRect);
	bool IsUIComponentSelectable(const UActorComponent* Component);
	USceneComponent* FindTopmostUIComponentAt(UWorld* World, float X, float Y);
	bool IsUIScreenTransformableComponent(const USceneComponent* Component);
	bool GetUIScreenTextBounds(const UUIScreenTextComponent* TextComponent, float& OutX, float& OutY, float& OutWidth, float& OutHeight);
	bool GetUIScreenComponentBounds(const USceneComponent* Component, float& OutX, float& OutY, float& OutWidth, float& OutHeight);
	bool GetUIScreenComponentPosition(const USceneComponent* Component, FVector& OutPosition);
	bool SetUIScreenComponentPosition(USceneComponent* Component, const FVector& InPosition);
	bool ConvertMouseToViewportPixelUnclamped(const ImVec2& Pos, const FRect& Rect, const FViewport* Viewport, float FallbackW, float FallbackH, float& OutX, float& OutY);
	bool TryConvertMouseToViewportPixel(const ImVec2& Pos,const FRect& Rect, const FViewport* Viewport, float FallbackW, float FallbackH, float& OutX, float& OutY);
	bool TryConvertPixelToScreen(float PX, float PY, const FRect& ScreenRect, const FViewport* Viewport, float FallbackW, float FallbackH, float& OutX, float& OutY);
	bool ProjectWorldToViewport(const FMatrix& ViewProjection, const FVector& WorldPosition, float ViewportWidth, float ViewportHeight, float& OutScreenX, float& OutScreenY, float& OutDepth);
	bool EditorRaycastAllVisiblePrimitives(UWorld* World, const FRay& Ray, FRayHitResult& OutHitResult, AActor*& OutActor);
	void BuildBoundingBoxCorners(const FBoundingBox& Bounds, FVector OutCorners[8]);
	AActor* FindScreenSpacePrimitiveAt(UWorld* World, const UCameraComponent* Camera, float MouseX, float MouseY, float ViewportW, float ViewportH, UPrimitiveComponent*& OutPrimitive);
	FVector FindClosestVertex(UWorld* World, const FRay& Ray, float MaxDistance = 10.0f);
}

FLevelEditorViewportClient::FLevelEditorViewportClient()
{
	SetupInput();
}

FLevelEditorViewportClient::~FLevelEditorViewportClient()
{
	ReleaseLevelInput();
}

UWorld* FLevelEditorViewportClient::GetWorld() const
{
	return GEngine ? GEngine->GetWorld() : nullptr;
}

bool FLevelEditorViewportClient::CanProcessCameraInput() const
{
	return !IsViewingFromLight();
}

void FLevelEditorViewportClient::SetupInput()
{
	ActionEditorOrbit = CreateLevelInputAction(EditorInputActions, "IA_EditorOrbit", EInputActionValueType::Axis2D);
	ActionEditorFocus = CreateLevelInputAction(EditorInputActions, "IA_EditorFocus", EInputActionValueType::Bool);
	ActionEditorDelete = CreateLevelInputAction(EditorInputActions, "IA_EditorDelete", EInputActionValueType::Bool);
	ActionEditorDuplicate = CreateLevelInputAction(EditorInputActions, "IA_EditorDuplicate", EInputActionValueType::Bool);
	ActionEditorToggleGizmoMode = CreateLevelInputAction(EditorInputActions, "IA_EditorToggleGizmoMode", EInputActionValueType::Bool);
	ActionEditorToggleCoordSystem = CreateLevelInputAction(EditorInputActions, "IA_EditorToggleCoordSystem", EInputActionValueType::Bool);
	ActionEditorEscape = CreateLevelInputAction(EditorInputActions, "IA_EditorEscape", EInputActionValueType::Bool);
	ActionEditorTogglePIE = CreateLevelInputAction(EditorInputActions, "IA_EditorTogglePIE", EInputActionValueType::Bool);
	ActionEditorDecreaseSnap = CreateLevelInputAction(EditorInputActions, "IA_EditorDecreaseSnap", EInputActionValueType::Bool);
	ActionEditorIncreaseSnap = CreateLevelInputAction(EditorInputActions, "IA_EditorIncreaseSnap", EInputActionValueType::Bool);
	ActionEditorVertexSnap = CreateLevelInputAction(EditorInputActions, "IA_EditorVertexSnap", EInputActionValueType::Bool);
	ActionEditorSnapToFloor = CreateLevelInputAction(EditorInputActions, "IA_EditorSnapToFloor", EInputActionValueType::Bool);
	ActionEditorSetBookmark = CreateLevelInputAction(EditorInputActions, "IA_EditorSetBookmark", EInputActionValueType::Float);
	ActionEditorJumpToBookmark = CreateLevelInputAction(EditorInputActions, "IA_EditorJumpToBookmark", EInputActionValueType::Float);
	ActionEditorSetViewportPerspective = CreateLevelInputAction(EditorInputActions, "IA_SetViewportPerspective", EInputActionValueType::Bool);
	ActionEditorSetViewportTop = CreateLevelInputAction(EditorInputActions, "IA_SetViewportTop", EInputActionValueType::Bool);
	ActionEditorSetViewportFront = CreateLevelInputAction(EditorInputActions, "IA_SetViewportFront", EInputActionValueType::Bool);
	ActionEditorSetViewportRight = CreateLevelInputAction(EditorInputActions, "IA_SetViewportRight", EInputActionValueType::Bool);
	ActionEditorToggleGridSnap = CreateLevelInputAction(EditorInputActions, "IA_ToggleGridSnap", EInputActionValueType::Bool);
	ActionEditorToggleRotationSnap = CreateLevelInputAction(EditorInputActions, "IA_ToggleRotationSnap", EInputActionValueType::Bool);
	ActionEditorToggleScaleSnap = CreateLevelInputAction(EditorInputActions, "IA_ToggleScaleSnap", EInputActionValueType::Bool);
	ActionEditorToggleGameView = CreateLevelInputAction(EditorInputActions, "IA_EditorToggleGameView", EInputActionValueType::Bool);

	LevelEditorMappingContext = new FInputMappingContext();
	LevelEditorMappingContext->ContextName = "IMC_LevelEditorViewport";

	LevelEditorMappingContext->AddMapping(ActionEditorOrbit, static_cast<int32>(EInputKey::MouseX));
	LevelEditorMappingContext->AddMapping(ActionEditorOrbit, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	LevelEditorMappingContext->AddMapping(ActionEditorFocus, 'F');
	LevelEditorMappingContext->AddMapping(ActionEditorDelete, VK_DELETE);
	LevelEditorMappingContext->AddMapping(ActionEditorDuplicate, 'D');
	LevelEditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, VK_SPACE);
	LevelEditorMappingContext->AddMapping(ActionEditorToggleCoordSystem, 'X');
	LevelEditorMappingContext->AddMapping(ActionEditorEscape, VK_ESCAPE);
	LevelEditorMappingContext->AddMapping(ActionEditorTogglePIE, VK_F8);

	LevelEditorMappingContext->AddMapping(ActionEditorDecreaseSnap, VK_OEM_4);
	LevelEditorMappingContext->AddMapping(ActionEditorIncreaseSnap, VK_OEM_6);
	LevelEditorMappingContext->AddMapping(ActionEditorVertexSnap, 'V');
	LevelEditorMappingContext->AddMapping(ActionEditorSnapToFloor, VK_END);

	LevelEditorMappingContext->AddMapping(ActionEditorSetViewportPerspective, 'G');
	LevelEditorMappingContext->AddMapping(ActionEditorSetViewportTop, 'J');
	LevelEditorMappingContext->AddMapping(ActionEditorSetViewportFront, 'H');
	LevelEditorMappingContext->AddMapping(ActionEditorSetViewportRight, 'K');

	LevelEditorMappingContext->AddMapping(ActionEditorToggleGridSnap, 'G');
	LevelEditorMappingContext->AddMapping(ActionEditorToggleRotationSnap, 'R');
	LevelEditorMappingContext->AddMapping(ActionEditorToggleScaleSnap, 'S');

	for (int32 i = 0; i < 10; ++i)
	{
		const int32 Key = '0' + i;
		LevelEditorMappingContext->AddMapping(ActionEditorSetBookmark, Key).Modifiers.push_back(new FModifierScale(FVector(static_cast<float>(i), 0, 0)));
		LevelEditorMappingContext->AddMapping(ActionEditorJumpToBookmark, Key).Modifiers.push_back(new FModifierScale(FVector(static_cast<float>(i), 0, 0)));
	}

	LevelEditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'Q');
	LevelEditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'W');
	LevelEditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'E');
	LevelEditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'R');

	LevelEditorMappingContext->AddMapping(ActionEditorToggleGameView, 'G');

	EnhancedInputManager.AddMappingContext(LevelEditorMappingContext, 1);
	EnhancedInputManager.BindAction(ActionEditorOrbit, ETriggerEvent::Triggered, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnOrbit(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorFocus, ETriggerEvent::Started, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnFocus(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorDelete, ETriggerEvent::Started, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnDelete(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorDuplicate, ETriggerEvent::Started, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnDuplicate(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorToggleGizmoMode, ETriggerEvent::Started, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnToggleGizmoMode(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorToggleCoordSystem, ETriggerEvent::Started, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnToggleCoordSystem(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorEscape, ETriggerEvent::Started, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnEscape(V, Snapshot); });
	EnhancedInputManager.BindAction(ActionEditorTogglePIE, ETriggerEvent::Started, [this](const FInputActionValue& V, const FInputSystemSnapshot& Snapshot) { OnTogglePIE(V, Snapshot); });

	EnhancedInputManager.BindAction(ActionEditorDecreaseSnap, ETriggerEvent::Started, [](const FInputActionValue&) {
		FEditorSettings& S = FEditorSettings::Get();
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) S.RotationSnapSize = (std::max)(1.0f, S.RotationSnapSize - 5.0f);
		else S.TranslationSnapSize = (std::max)(0.1f, S.TranslationSnapSize / 2.0f);
	});
	EnhancedInputManager.BindAction(ActionEditorIncreaseSnap, ETriggerEvent::Started, [](const FInputActionValue&) {
		FEditorSettings& S = FEditorSettings::Get();
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) S.RotationSnapSize = (std::min)(90.0f, S.RotationSnapSize + 5.0f);
		else S.TranslationSnapSize = (std::min)(1000.0f, S.TranslationSnapSize * 2.0f);
	});
	EnhancedInputManager.BindAction(ActionEditorSnapToFloor, ETriggerEvent::Started, [this](const FInputActionValue&) {
		if (!SelectionManager)
		{
			return;
		}

		UWorld* W = GetWorld();
		for (AActor* Actor : SelectionManager->GetSelectedActors())
		{
			if (!Actor)
			{
				continue;
			}

			FRay DownRay(Actor->GetActorLocation(), FVector(0, 0, -1));
			FRayHitResult Hit;
			AActor* HitActor = nullptr;
			if (W && W->RaycastPrimitives(DownRay, Hit, HitActor))
			{
				Actor->SetActorLocation(DownRay.Origin + DownRay.Direction * Hit.Distance);
			}
		}
		if (Gizmo)
		{
			Gizmo->UpdateGizmoTransform();
		}
	});
	EnhancedInputManager.BindAction(ActionEditorSetBookmark, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (FInputManager::Get().IsKeyDown(VK_CONTROL) && Camera)
		{
			const int32 Index = static_cast<int32>(V.Get());
			if (Index >= 0 && Index < 10)
			{
				GCameraBookmarks[Index] = { Camera->GetWorldLocation(), Camera->GetRelativeRotation(), true };
			}
		}
	});
	EnhancedInputManager.BindAction(ActionEditorJumpToBookmark, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (!FInputManager::Get().IsKeyDown(VK_CONTROL) && Camera)
		{
			const int32 Index = static_cast<int32>(V.Get());
			if (Index >= 0 && Index < 10 && GCameraBookmarks[Index].bValid)
			{
				const FCameraBookmark& BM = GCameraBookmarks[Index];
				FocusStartLoc = Camera->GetWorldLocation();
				FocusStartRot = Camera->GetRelativeRotation();
				FocusEndLoc = BM.Location;
				FocusEndRot = BM.Rotation;
				bIsFocusAnimating = true;
				FocusAnimTimer = 0.0f;
			}
		}
	});
	EnhancedInputManager.BindAction(ActionEditorSetViewportPerspective, ETriggerEvent::Started, [this](const FInputActionValue&) {
		if (FInputManager::Get().IsKeyDown(VK_MENU)) SetViewportType(ELevelViewportType::Perspective);
	});
	EnhancedInputManager.BindAction(ActionEditorSetViewportTop, ETriggerEvent::Started, [this](const FInputActionValue&) {
		if (FInputManager::Get().IsKeyDown(VK_MENU)) SetViewportType(ELevelViewportType::Top);
	});
	EnhancedInputManager.BindAction(ActionEditorSetViewportFront, ETriggerEvent::Started, [this](const FInputActionValue&) {
		if (FInputManager::Get().IsKeyDown(VK_MENU)) SetViewportType(ELevelViewportType::Front);
	});
	EnhancedInputManager.BindAction(ActionEditorSetViewportRight, ETriggerEvent::Started, [this](const FInputActionValue&) {
		if (FInputManager::Get().IsKeyDown(VK_MENU)) SetViewportType(ELevelViewportType::Right);
	});
	EnhancedInputManager.BindAction(ActionEditorToggleGridSnap, ETriggerEvent::Started, [](const FInputActionValue&) {
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) FEditorSettings::Get().bEnableTranslationSnap = !FEditorSettings::Get().bEnableTranslationSnap;
	});
	EnhancedInputManager.BindAction(ActionEditorToggleRotationSnap, ETriggerEvent::Started, [](const FInputActionValue&) {
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) FEditorSettings::Get().bEnableRotationSnap = !FEditorSettings::Get().bEnableRotationSnap;
	});
	EnhancedInputManager.BindAction(ActionEditorToggleScaleSnap, ETriggerEvent::Started, [](const FInputActionValue&) {
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) FEditorSettings::Get().bEnableScaleSnap = !FEditorSettings::Get().bEnableScaleSnap;
	});
	EnhancedInputManager.BindAction(ActionEditorToggleGameView, ETriggerEvent::Started, [this](const FInputActionValue&, const FInputSystemSnapshot& Snapshot) {
		if (!Snapshot.IsKeyDown(VK_MENU) && !Snapshot.IsKeyDown(VK_SHIFT))
		{
			RenderOptions.bGameView = !RenderOptions.bGameView;
		}
	});
}

void FLevelEditorViewportClient::ReleaseLevelInput()
{
	EnhancedInputManager.RemoveMappingContext(LevelEditorMappingContext);
	DeleteMappingContext(LevelEditorMappingContext);
	LevelEditorMappingContext = nullptr;

	for (FInputAction* Action : EditorInputActions)
	{
		delete Action;
	}
	EditorInputActions.clear();
}

void FLevelEditorViewportClient::OnOrbit(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	if (!Snapshot.IsKeyDown(VK_MENU))
	{
		return;
	}

	if (Snapshot.IsMouseButtonDown(VK_LBUTTON))
	{
		if (SelectionManager && SelectionManager->GetPrimarySelection() && Camera)
		{
			const FVector Pivot = SelectionManager->GetPrimarySelection()->GetActorLocation();
			const FVector CameraPos = Camera->GetWorldLocation();
			const float Dist = FVector::Distance(CameraPos, Pivot);
			const float Sensitivity = 0.25f;
			FRotator Rotation = Camera->GetRelativeRotation();
			Rotation.Yaw += Value.GetVector().X * Sensitivity;
			Rotation.Pitch = Clamp(Rotation.Pitch + Value.GetVector().Y * Sensitivity, -89.0f, 89.0f);
			const FVector NewPos = Pivot - Rotation.ToVector() * (Dist > 0.1f ? Dist : 5.0f);
			Camera->SetWorldLocation(NewPos);
			Camera->SetRelativeRotation(Rotation);
			SyncCameraSmoothingTarget();
		}
		else
		{
			EditorRotateAccumulator = EditorRotateAccumulator + Value.GetVector();
		}
	}
}

void FLevelEditorViewportClient::OnFocus(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	(void)Value;
	if (Snapshot.IsMouseButtonDown(VK_RBUTTON))
	{
		return;
	}

	if (SelectionManager)
	{
		FocusActor(SelectionManager->GetPrimarySelection());
	}
}

void FLevelEditorViewportClient::OnDelete(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	(void)Value;
	(void)Snapshot;
	if (SelectionManager)
	{
		SelectionManager->DeleteSelectedActors();
	}
}

void FLevelEditorViewportClient::OnDuplicate(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	(void)Value;
	if (!SelectionManager || !Snapshot.IsKeyDown(VK_CONTROL))
	{
		return;
	}

	DuplicateSelectedActors(SelectionManager, Gizmo, FVector(0.1f, 0.1f, 0.1f));
}

void FLevelEditorViewportClient::OnToggleGizmoMode(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	(void)Value;
	if (Snapshot.IsMouseButtonDown(VK_RBUTTON) || !Gizmo)
	{
		return;
	}

	if (Snapshot.IsKeyPressed('W')) Gizmo->UpdateGizmoMode(EGizmoMode::Translate);
	else if (Snapshot.IsKeyPressed('E')) Gizmo->UpdateGizmoMode(EGizmoMode::Rotate);
	else if (Snapshot.IsKeyPressed('R')) Gizmo->UpdateGizmoMode(EGizmoMode::Scale);
	else if (Snapshot.IsKeyPressed('Q')) Gizmo->UpdateGizmoMode(EGizmoMode::Select);
	else Gizmo->SetNextMode();

	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		EditorEngine->ApplyTransformSettingsToGizmo();
	}
}

void FLevelEditorViewportClient::OnToggleCoordSystem(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	(void)Value;
	if (!Snapshot.IsKeyDown(VK_CONTROL))
	{
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			EditorEngine->ToggleCoordSystem();
		}
	}
}

void FLevelEditorViewportClient::OnEscape(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	(void)Value;
	(void)Snapshot;
	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor())
		{
			EditorEngine->RequestEndPlayMap();
		}
	}
}

void FLevelEditorViewportClient::OnTogglePIE(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot)
{
	(void)Value;
	(void)Snapshot;
	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor())
		{
			EditorEngine->TogglePIEControlMode();
		}
	}
}

void FLevelEditorViewportClient::SetLightViewOverride(ULightComponentBase* Light)
{
	LightViewOverride = Light;
	PointLightFaceIndex = 0;
	if (Light && SelectionManager)
	{
		SelectionManager->ClearSelection();
	}
}

void FLevelEditorViewportClient::ClearLightViewOverride()
{
	LightViewOverride = nullptr;
}

bool FLevelEditorViewportClient::FocusActor(AActor* Actor)
{
	if (!Actor || !Camera)
	{
		return false;
	}

	const FVector TargetLoc = Actor->GetActorLocation();
	const FVector CameraForward = Camera->GetForwardVector();
	const FVector OriginalLoc = Camera->GetWorldLocation();
	const FRotator OriginalRot = Camera->GetRelativeRotation();

	float FocusDistance = 5.0f;
	if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		const FVector Extent = RootPrim->GetWorldBoundingBox().GetExtent();
		const float MaxDim = (std::max)({ Extent.X, Extent.Y, Extent.Z });
		FocusDistance = (std::max)(5.0f, MaxDim * 2.5f);
	}

	const FVector NewCameraLoc = TargetLoc - CameraForward * FocusDistance;
	Camera->SetWorldLocation(NewCameraLoc);
	Camera->LookAt(TargetLoc);
	const FRotator TargetRot = Camera->GetRelativeRotation();
	Camera->SetWorldLocation(OriginalLoc);
	Camera->SetRelativeRotation(OriginalRot);

	bIsFocusAnimating = true;
	FocusAnimTimer = 0.0f;
	FocusStartLoc = OriginalLoc;
	FocusStartRot = OriginalRot;
	FocusEndLoc = NewCameraLoc;
	FocusEndRot = TargetRot;
	return true;
}

void FLevelEditorViewportClient::SetViewportType(ELevelViewportType NewType)
{
	if (!Camera)
	{
		return;
	}

	RenderOptions.ViewportType = NewType;
	if (NewType == ELevelViewportType::Perspective)
	{
		Camera->SetOrthographic(false);
		SyncCameraSmoothingTarget();
		return;
	}
	if (NewType == ELevelViewportType::FreeOrthographic)
	{
		Camera->SetOrthographic(true);
		SyncCameraSmoothingTarget();
		return;
	}

	Camera->SetOrthographic(true);
	constexpr float OrthoDistance = 50.0f;
	FVector Position = FVector(0, 0, 0);
	FVector Rotation = FVector(0, 0, 0);
	switch (NewType)
	{
	case ELevelViewportType::Top: Position = FVector(0, 0, OrthoDistance); Rotation = FVector(0, 90.0f, 0); break;
	case ELevelViewportType::Bottom: Position = FVector(0, 0, -OrthoDistance); Rotation = FVector(0, -90.0f, 0); break;
	case ELevelViewportType::Front: Position = FVector(OrthoDistance, 0, 0); Rotation = FVector(0, 0, 180.0f); break;
	case ELevelViewportType::Back: Position = FVector(-OrthoDistance, 0, 0); Rotation = FVector(0, 0, 0.0f); break;
	case ELevelViewportType::Left: Position = FVector(0, -OrthoDistance, 0); Rotation = FVector(0, 0, 90.0f); break;
	case ELevelViewportType::Right: Position = FVector(0, OrthoDistance, 0); Rotation = FVector(0, 0, -90.0f); break;
	default: break;
	}
	Camera->SetRelativeLocation(Position);
	Camera->SetRelativeRotation(Rotation);
	SyncCameraSmoothingTarget();
}

void FLevelEditorViewportClient::Tick(float DeltaTime)
{
	if (!bIsActive)
	{
		return;
	}

	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor() && EditorEngine->IsPIEPossessedMode())
		{
			if (UGameViewportClient* GameViewportClient = EditorEngine->GetGameViewportClient())
			{
				GameViewportClient->SetViewport(Viewport);
				GameViewportClient->SetCursorClipRect(ViewportScreenRect);

				if (!GameViewportClient->HasPossessedTarget())
				{
					if (UWorld* World = EditorEngine->GetWorld())
					{
						GameViewportClient->Possess(World->GetActiveCamera());
					}

					if (!GameViewportClient->HasPossessedTarget())
					{
						GameViewportClient->Possess(Camera);
					}
				}
			}
			return;
		}
	}

	FEditorViewportClient::Tick(DeltaTime);
}

bool FLevelEditorViewportClient::HandleInputSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	if (!bIsActive && !bIsHovered && !bCameraInputCaptured && !bDraggingUIScreenGizmo && !bIsMarqueeSelecting)
	{
		return false;
	}

	TickInput(Snapshot, DeltaTime);
	TickInteraction(Snapshot, DeltaTime);
	return true;
}

void FLevelEditorViewportClient::TickEditorShortcuts(const FInputSystemSnapshot& Snapshot)
{
	UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
	if (!EditorEngine)
	{
		return;
	}

	if (EditorEngine->IsPlayingInEditor() && Snapshot.IsKeyPressed(VK_ESCAPE))
	{
		EditorEngine->RequestEndPlayMap();
	}

	const bool bAllowKeyboardInput = !Snapshot.IsGuiUsingKeyboard() && !ImGui::GetIO().WantTextInput;
	if (!bAllowKeyboardInput)
	{
		return;
	}

	if (SelectionManager && Snapshot.IsKeyPressed(VK_DELETE))
	{
		EditorEngine->BeginTrackedSceneChange();
		SelectionManager->DeleteSelectedActors();
		EditorEngine->CommitTrackedSceneChange();
		return;
	}

	if (!Snapshot.IsKeyDown(VK_CONTROL) && Snapshot.IsKeyPressed('X'))
	{
		EditorEngine->ToggleCoordSystem();
		return;
	}

	if (SelectionManager && Snapshot.IsKeyPressed('F'))
	{
		FocusActor(SelectionManager->GetPrimarySelection());
	}
}

void FLevelEditorViewportClient::TickInteraction(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	(void)DeltaTime;
	if (!Camera || !Gizmo || !GetWorld())
	{
		return;
	}

	Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());
	Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(RenderOptions.ViewportType, Gizmo->GetMode()));

	uint32 CursorViewportX = 0;
	uint32 CursorViewportY = 0;
	const bool bCursorInViewport = GetCursorViewportPosition(CursorViewportX, CursorViewportY);
	if (Snapshot.IsGuiUsingMouse() && !bCursorInViewport && !Gizmo->IsHolding() && !bIsMarqueeSelecting)
	{
		return;
	}

	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	HoveredUIScreenGizmoAxis = HasUIScreenTranslateGizmo() ? HitTestUIScreenTranslateGizmo(MousePos) : 0;
	const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
	const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
	float LocalMouseX = 0.0f;
	float LocalMouseY = 0.0f;
	if (!ConvertMouseToViewportPixelUnclamped(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, LocalMouseX, LocalMouseY))
	{
		LocalMouseX = MousePos.x - ViewportScreenRect.X;
		LocalMouseY = MousePos.y - ViewportScreenRect.Y;
	}

	const FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
	FRayHitResult HitResult;
	const bool bCanInteractWithGizmo = SelectionManager && SelectionManager->GetSelectedComponent() && Gizmo->HasTarget();
	const bool bGizmoHit = bCanInteractWithGizmo && FRayUtils::RaycastComponent(Gizmo, Ray, HitResult);

	if (Snapshot.IsKeyPressed(FInputManager::MOUSE_LEFT) && bIsHovered)
	{
		FInputRouter::Get().SetMouseCapturedViewport(this);
		if (Snapshot.IsKeyDown(VK_CONTROL) && Snapshot.IsKeyDown(VK_MENU))
		{
			bIsMarqueeSelecting = true;
			MarqueeStartPos = FVector(MousePos.x, MousePos.y, 0);
			MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0);
		}
		else
		{
			if (Snapshot.IsKeyDown(VK_MENU) && bGizmoHit && SelectionManager && !SelectionManager->IsEmpty())
			{
				DuplicateSelectedActors(SelectionManager, Gizmo);
			}
			HandleDragStart(Snapshot, Ray);
		}
	}
	else if (Snapshot.IsMouseButtonDown(FInputManager::MOUSE_LEFT))
	{
		if (bIsMarqueeSelecting)
		{
			MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0);
		}
		else if (bDraggingUIScreenGizmo)
		{
			UpdateUIScreenTranslateDrag(MousePos);
		}
		else
		{
			if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
			{
				Gizmo->SetHolding(true);
			}
			if (Gizmo->IsHolding())
			{
				if (Snapshot.IsKeyDown('V'))
				{
					const FVector BestV = FindClosestVertex(GetWorld(), Ray, 2.0f);
					if (!BestV.IsNearlyZero())
					{
						Gizmo->SetTargetLocation(BestV);
						TargetLocation = BestV;
					}
				}
				Gizmo->UpdateDrag(Ray);
			}
		}
	}
	else if (Snapshot.IsKeyReleased(FInputManager::MOUSE_LEFT))
	{
		if (bDraggingUIScreenGizmo)
		{
			EndUIScreenTranslateDrag(true);
		}
		else if (bIsMarqueeSelecting)
		{
			bIsMarqueeSelecting = false;
			ProcessMarqueeSelection(Snapshot.IsKeyDown(VK_CONTROL));
		}
		else
		{
			Gizmo->DragEnd();
			if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
			{
				EditorEngine->CommitTrackedTransformChange();
			}
		}

		if (!bCameraInputCaptured && !Snapshot.IsMouseButtonDown(VK_RBUTTON))
		{
			FInputRouter::Get().ReleaseMouseCapture(this);
		}
	}
	else if (Snapshot.IsKeyReleased(VK_LBUTTON))
	{
		if (bDraggingUIScreenGizmo)
		{
			EndUIScreenTranslateDrag(true);
		}
		Gizmo->SetPressedOnHandle(false);
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			EditorEngine->CommitTrackedTransformChange();
		}
		bIsMarqueeSelecting = false;
		if (!bCameraInputCaptured && !Snapshot.IsMouseButtonDown(VK_RBUTTON))
		{
			FInputRouter::Get().ReleaseMouseCapture(this);
		}
	}
}

void FLevelEditorViewportClient::ProcessMarqueeSelection(bool bAppendSelection)
{
	if (!Camera || !SelectionManager)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float MinX = (std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X);
	const float MaxX = (std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X);
	const float MinY = (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y);
	const float MaxY = (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y);
	if (std::abs(MaxX - MinX) <= 2.0f && std::abs(MaxY - MinY) <= 2.0f)
	{
		return;
	}

	if (!bAppendSelection)
	{
		SelectionManager->ClearSelection();
	}

	const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
	const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
	const FMatrix VP = Camera->GetViewProjectionMatrix();
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || !Actor->IsVisible() || Actor->IsA<UGizmoComponent>())
		{
			continue;
		}

		const FVector ClipSpace = VP.TransformPositionWithW(Actor->GetActorLocation());
		const float ScreenX = (ClipSpace.X * 0.5f + 0.5f) * VPWidth + ViewportScreenRect.X;
		const float ScreenY = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * VPHeight + ViewportScreenRect.Y;
		if (ScreenX >= MinX && ScreenX <= MaxX && ScreenY >= MinY && ScreenY <= MaxY)
		{
			SelectionManager->ToggleSelect(Actor);
		}
	}
}

void FLevelEditorViewportClient::HandleDragStart(const FInputSystemSnapshot& Snapshot, const FRay& Ray)
{
	if (!bIsHovered)
	{
		return;
	}
	if (BeginUIScreenTranslateDrag(ImGui::GetIO().MousePos))
	{
		return;
	}

	FScopeCycleCounter PickCounter;
	FRayHitResult HitResult{};
	const bool bCanInteractWithGizmo = SelectionManager && SelectionManager->GetSelectedComponent() && Gizmo->HasTarget();
	if (bCanInteractWithGizmo && FRayUtils::RaycastComponent(Gizmo, Ray, HitResult))
	{
		if (SelectionManager)
		{
			for (AActor* Actor : SelectionManager->GetSelectedActors())
			{
				if (Actor && Actor->IsActorMovementLocked())
				{
					Gizmo->SetPressedOnHandle(false);
					return;
				}
			}
		}
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			EditorEngine->BeginTrackedTransformChange();
		}
		Gizmo->SetPressedOnHandle(true);
	}
	else
	{
		uint32 CursorX = 0;
		uint32 CursorY = 0;
		USceneComponent* HitUIComponent = GetCursorViewportPosition(CursorX, CursorY)
			? FindTopmostUIComponentAt(GetWorld(), static_cast<float>(CursorX), static_cast<float>(CursorY))
			: nullptr;
		if (HitUIComponent)
		{
			AActor* HitActor = HitUIComponent->GetOwner();
			const bool bCtrlHeld = Snapshot.IsKeyDown(VK_CONTROL);
			if (HitActor)
			{
				if (bCtrlHeld)
				{
					const bool bWasSelected = SelectionManager->IsSelected(HitActor);
					SelectionManager->ToggleSelect(HitActor);
					if (!bWasSelected)
					{
						SelectionManager->SelectComponent(HitUIComponent);
					}
				}
				else
				{
					if (SelectionManager->GetPrimarySelection() != HitActor)
					{
						SelectionManager->Select(HitActor);
					}
					SelectionManager->SelectComponent(HitUIComponent);
				}
			}
		}
		else
		{
			AActor* BestActor = nullptr;
			if (UWorld* W = GetWorld())
			{
				W->RaycastPrimitives(Ray, HitResult, BestActor);
				if (!BestActor)
				{
					EditorRaycastAllVisiblePrimitives(W, Ray, HitResult, BestActor);
				}
				if (!BestActor && Camera)
				{
					const ImVec2 MousePos = ImGui::GetIO().MousePos;
					float LocalMouseX = 0.0f;
					float LocalMouseY = 0.0f;
					if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, LocalMouseX, LocalMouseY))
					{
						UPrimitiveComponent* ScreenHitPrimitive = nullptr;
						const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
						const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
						BestActor = FindScreenSpacePrimitiveAt(W, Camera, LocalMouseX, LocalMouseY, VPWidth, VPHeight, ScreenHitPrimitive);
						if (ScreenHitPrimitive)
						{
							HitResult.HitComponent = ScreenHitPrimitive;
						}
					}
				}
			}

			const bool bCtrlHeld = Snapshot.IsKeyDown(VK_CONTROL);
			if (!BestActor)
			{
				if (!bCtrlHeld)
				{
					SelectionManager->ClearSelection();
				}
			}
			else if (bCtrlHeld)
			{
				SelectionManager->ToggleSelect(BestActor);
			}
			else if (SelectionManager->GetPrimarySelection() == BestActor)
			{
				if (HitResult.HitComponent)
				{
					SelectionManager->SelectComponent(HitResult.HitComponent);
				}
			}
			else
			{
				SelectionManager->Select(BestActor);
			}
		}
	}

	if (OverlayStatSystem)
	{
		const uint64 PickCycles = PickCounter.Finish();
		const double ElapsedMs = FPlatformTime::ToMilliseconds(PickCycles);
		OverlayStatSystem->RecordPickingAttempt(ElapsedMs);
	}
}

bool FLevelEditorViewportClient::HasUIScreenTranslateGizmo() const
{
	if (!SelectionManager || !Gizmo || Gizmo->GetMode() != EGizmoMode::Translate)
	{
		return false;
	}

	if (const UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor())
		{
			return false;
		}
	}

	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	return SelectedComponent && IsUIScreenTransformableComponent(SelectedComponent);
}

int32 FLevelEditorViewportClient::HitTestUIScreenTranslateGizmo(const ImVec2& MousePos) const
{
	if (!HasUIScreenTranslateGizmo())
	{
		return static_cast<int32>(EUIScreenGizmoAxis::None);
	}

	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	if (!GetUIScreenComponentBounds(SelectedComponent, X, Y, Width, Height))
	{
		return static_cast<int32>(EUIScreenGizmoAxis::None);
	}

	float CenterX = X + Width * 0.5f;
	float CenterY = Y + Height * 0.5f;
	if (!TryConvertPixelToScreen(CenterX, CenterY, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, CenterX, CenterY))
	{
		CenterX += ViewportScreenRect.X;
		CenterY += ViewportScreenRect.Y;
	}

	const ImVec2 Local(MousePos.x - CenterX, MousePos.y - CenterY);
	const float CenterHalf = 8.0f;
	const float AxisThickness = 6.0f;
	const float AxisLength = 48.0f;

	if (std::abs(Local.x) <= CenterHalf && std::abs(Local.y) <= CenterHalf)
	{
		return static_cast<int32>(EUIScreenGizmoAxis::XY);
	}
	if (Local.x >= CenterHalf && Local.x <= AxisLength && std::abs(Local.y) <= AxisThickness)
	{
		return static_cast<int32>(EUIScreenGizmoAxis::X);
	}
	if (Local.y >= CenterHalf && Local.y <= AxisLength && std::abs(Local.x) <= AxisThickness)
	{
		return static_cast<int32>(EUIScreenGizmoAxis::Y);
	}
	return static_cast<int32>(EUIScreenGizmoAxis::None);
}

bool FLevelEditorViewportClient::BeginUIScreenTranslateDrag(const ImVec2& MousePos)
{
	if (!HasUIScreenTranslateGizmo())
	{
		return false;
	}

	const int32 HitAxis = HitTestUIScreenTranslateGizmo(MousePos);
	if (HitAxis == static_cast<int32>(EUIScreenGizmoAxis::None))
	{
		return false;
	}

	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		EditorEngine->BeginTrackedTransformChange();
	}

	ActiveUIScreenGizmoAxis = HitAxis;
	float ViewportMouseX = 0.0f;
	float ViewportMouseY = 0.0f;
	if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, ViewportMouseX, ViewportMouseY))
	{
		LastUIScreenGizmoMousePos = ImVec2(ViewportMouseX, ViewportMouseY);
	}
	else
	{
		LastUIScreenGizmoMousePos = MousePos;
	}
	bDraggingUIScreenGizmo = true;
	return true;
}

void FLevelEditorViewportClient::UpdateUIScreenTranslateDrag(const ImVec2& MousePos)
{
	if (!bDraggingUIScreenGizmo || !SelectionManager)
	{
		return;
	}

	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	if (!IsUIScreenTransformableComponent(SelectedComponent))
	{
		return;
	}

	FVector ScreenPosition;
	if (!GetUIScreenComponentPosition(SelectedComponent, ScreenPosition))
	{
		return;
	}

	float ViewportMouseX = 0.0f;
	float ViewportMouseY = 0.0f;
	ImVec2 CurrentMouseInViewport = MousePos;
	if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, ViewportMouseX, ViewportMouseY))
	{
		CurrentMouseInViewport = ImVec2(ViewportMouseX, ViewportMouseY);
	}
	else
	{
		return;
	}

	const ImVec2 Delta(CurrentMouseInViewport.x - LastUIScreenGizmoMousePos.x, CurrentMouseInViewport.y - LastUIScreenGizmoMousePos.y);
	switch (static_cast<EUIScreenGizmoAxis>(ActiveUIScreenGizmoAxis))
	{
	case EUIScreenGizmoAxis::X:
		ScreenPosition.X += Delta.x;
		break;
	case EUIScreenGizmoAxis::Y:
		ScreenPosition.Y += Delta.y;
		break;
	case EUIScreenGizmoAxis::XY:
		ScreenPosition.X += Delta.x;
		ScreenPosition.Y += Delta.y;
		break;
	default:
		return;
	}

	SetUIScreenComponentPosition(SelectedComponent, ScreenPosition);
	LastUIScreenGizmoMousePos = CurrentMouseInViewport;
}

void FLevelEditorViewportClient::EndUIScreenTranslateDrag(bool bCommitChange)
{
	if (!bDraggingUIScreenGizmo)
	{
		return;
	}

	bDraggingUIScreenGizmo = false;
	ActiveUIScreenGizmoAxis = static_cast<int32>(EUIScreenGizmoAxis::None);
	if (bCommitChange)
	{
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			EditorEngine->CommitTrackedTransformChange();
		}
	}
}

void FLevelEditorViewportClient::UpdateLayoutRect()
{
	if (!LayoutWindow)
	{
		return;
	}

	const FRect& R = LayoutWindow->GetRect();
	ViewportScreenRect = R;
	if (!Viewport)
	{
		return;
	}

	if (FProjectSettings::Get().Game.bLockWindowResolution)
	{
		const uint32 TargetW = (std::max)(320u, FProjectSettings::Get().Game.WindowWidth);
		const uint32 TargetH = (std::max)(240u, FProjectSettings::Get().Game.WindowHeight);
		if (Camera)
		{
			Camera->OnResize(static_cast<int32>(TargetW), static_cast<int32>(TargetH));
		}
		if (Viewport->GetWidth() != TargetW || Viewport->GetHeight() != TargetH)
		{
			Viewport->RequestResize(TargetW, TargetH);
		}

		if (R.Width > 0.0f && R.Height > 0.0f)
		{
			const float Scale = (std::min)(R.Width / static_cast<float>(TargetW), R.Height / static_cast<float>(TargetH));
			const float DrawW = static_cast<float>(TargetW) * Scale;
			const float DrawH = static_cast<float>(TargetH) * Scale;
			ViewportScreenRect.X = R.X + (R.Width - DrawW) * 0.5f;
			ViewportScreenRect.Y = R.Y + (R.Height - DrawH) * 0.5f;
			ViewportScreenRect.Width = DrawW;
			ViewportScreenRect.Height = DrawH;
		}
		SyncPIEViewportRect(Viewport, ViewportScreenRect);
		return;
	}

	const uint32 SlotW = static_cast<uint32>(R.Width);
	const uint32 SlotH = static_cast<uint32>(R.Height);
	if (Camera && SlotW > 0 && SlotH > 0)
	{
		Camera->OnResize(static_cast<int32>(SlotW), static_cast<int32>(SlotH));
	}
	if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
	{
		Viewport->RequestResize(SlotW, SlotH);
	}
	SyncPIEViewportRect(Viewport, ViewportScreenRect);
}

void FLevelEditorViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
	if (!Viewport || !Viewport->GetSRV())
	{
		return;
	}

	const FRect& R = ViewportScreenRect;
	if (R.Width <= 0 || R.Height <= 0)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 Min(R.X, R.Y);
	const ImVec2 Max(R.X + R.Width, R.Y + R.Height);
	DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);
	if (bIsActiveViewport)
	{
		constexpr float ActiveBorderThickness = 4.0f;
		const float BorderInset = ActiveBorderThickness * 0.5f;
		ImU32 BorderColor = IM_COL32(255, 165, 0, 220);
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			if (EditorEngine->IsPlayingInEditor())
			{
				BorderColor = EditorEngine->IsGamePaused()
					? EditorAccentColor::ToU32()
					: IM_COL32(52, 199, 89, 255);
			}
		}
		if (R.Width > ActiveBorderThickness && R.Height > ActiveBorderThickness)
		{
			DrawList->AddRect(
				ImVec2(Min.x + BorderInset, Min.y + BorderInset),
				ImVec2(Max.x - BorderInset, Max.y - BorderInset),
				BorderColor,
				0.0f,
				0,
				ActiveBorderThickness);
		}
	}

	if (bIsMarqueeSelecting)
	{
		ImDrawList* ForegroundDrawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
		const ImVec2 RectMin((std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X), (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));
		const ImVec2 RectMax((std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X), (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));
		ForegroundDrawList->AddRect(RectMin, RectMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 5.0f);
	}

	DrawUIScreenTranslateGizmo();
}

void FLevelEditorViewportClient::DrawUIScreenTranslateGizmo()
{
	if (!HasUIScreenTranslateGizmo())
	{
		return;
	}

	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	if (!GetUIScreenComponentBounds(SelectedComponent, X, Y, Width, Height))
	{
		return;
	}

	float CenterX = X + Width * 0.5f;
	float CenterY = Y + Height * 0.5f;
	if (!TryConvertPixelToScreen(CenterX, CenterY, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, CenterX, CenterY))
	{
		CenterX += ViewportScreenRect.X;
		CenterY += ViewportScreenRect.Y;
	}

	const float AxisLength = 48.0f;
	const float CenterHalf = 8.0f;
	const float Thickness = 3.0f;
	const bool bHoverX = HoveredUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::X);
	const bool bHoverY = HoveredUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::Y);
	const bool bHoverXY = HoveredUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::XY);
	const bool bActiveX = ActiveUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::X);
	const bool bActiveY = ActiveUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::Y);
	const bool bActiveXY = ActiveUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::XY);
	const ImU32 XColor = (bHoverX || bActiveX) ? IM_COL32(255, 120, 120, 255) : IM_COL32(230, 70, 70, 255);
	const ImU32 YColor = (bHoverY || bActiveY) ? IM_COL32(120, 255, 160, 255) : IM_COL32(70, 210, 110, 255);
	const ImU32 CenterColor = (bHoverXY || bActiveXY) ? IM_COL32(120, 190, 255, 255) : IM_COL32(26, 138, 245, 255);

	ImDrawList* DrawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
	const ImVec2 Center(CenterX, CenterY);
	DrawList->AddLine(Center, ImVec2(CenterX + AxisLength, CenterY), XColor, Thickness);
	DrawList->AddTriangleFilled(ImVec2(CenterX + AxisLength + 8.0f, CenterY), ImVec2(CenterX + AxisLength - 4.0f, CenterY - 6.0f), ImVec2(CenterX + AxisLength - 4.0f, CenterY + 6.0f), XColor);
	DrawList->AddLine(Center, ImVec2(CenterX, CenterY + AxisLength), YColor, Thickness);
	DrawList->AddTriangleFilled(ImVec2(CenterX, CenterY + AxisLength + 8.0f), ImVec2(CenterX - 6.0f, CenterY + AxisLength - 4.0f), ImVec2(CenterX + 6.0f, CenterY + AxisLength - 4.0f), YColor);
	DrawList->AddRectFilled(ImVec2(CenterX - CenterHalf, CenterY - CenterHalf), ImVec2(CenterX + CenterHalf, CenterY + CenterHalf), CenterColor, 2.0f);
	DrawList->AddRect(ImVec2(CenterX - CenterHalf, CenterY - CenterHalf), ImVec2(CenterX + CenterHalf, CenterY + CenterHalf), IM_COL32(255, 255, 255, 220), 2.0f, 0, 1.5f);
}

namespace
{
	FInputAction* CreateLevelInputAction(TArray<FInputAction*>& Actions, const char* Name, EInputActionValueType ValueType)
	{
		FInputAction* Action = new FInputAction(Name, ValueType);
		Actions.push_back(Action);
		return Action;
	}

	void DuplicateSelectedActors(FSelectionManager* SelectionManager, UGizmoComponent* Gizmo, const FVector& OffsetStep)
	{
		if (!SelectionManager || SelectionManager->IsEmpty())
		{
			return;
		}

		TArray<AActor*> NewSelection;
		int32 DuplicateIndex = 0;
		for (AActor* Src : SelectionManager->GetSelectedActors())
		{
			if (!Src)
			{
				continue;
			}

			AActor* Dup = Cast<AActor>(Src->Duplicate(nullptr));
			if (!Dup)
			{
				continue;
			}

			if (!OffsetStep.IsNearlyZero())
			{
				Dup->AddActorWorldOffset(OffsetStep * static_cast<float>(DuplicateIndex + 1));
			}
			NewSelection.push_back(Dup);
			++DuplicateIndex;
		}

		if (NewSelection.empty())
		{
			return;
		}

		SelectionManager->ClearSelection();
		for (AActor* Actor : NewSelection)
		{
			SelectionManager->ToggleSelect(Actor);
		}
		if (Gizmo)
		{
			Gizmo->UpdateGizmoTransform();
		}
	}

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

	void SyncPIEViewportRect(FViewport* Viewport, const FRect& ViewportScreenRect)
	{
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			if (EditorEngine->IsPlayingInEditor())
			{
				if (UGameViewportClient* GameViewportClient = EditorEngine->GetGameViewportClient())
				{
					if (GameViewportClient->GetViewport() == Viewport)
					{
						GameViewportClient->SetViewport(Viewport);
						GameViewportClient->SetCursorClipRect(ViewportScreenRect);
					}
				}
			}
		}
	}

	bool IsUIComponentSelectable(const UActorComponent* Component)
	{
		return Component
			&& !Component->IsHiddenInComponentTree()
			&& !Component->IsEditorOnlyComponent()
			&& Component->SupportsUIScreenPicking();
	}

	USceneComponent* FindTopmostUIComponentAt(UWorld* World, float X, float Y)
	{
		if (!World)
		{
			return nullptr;
		}

		USceneComponent* BestComponent = nullptr;
		int32 BestZOrder = INT_MIN;
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !Actor->IsVisible())
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!IsUIComponentSelectable(Component))
				{
					continue;
				}

				USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
				if (!SceneComponent || !Component->HitTestUIScreenPoint(X, Y))
				{
					continue;
				}

				const int32 ZOrder = Component->GetUIScreenPickingZOrder();
				if (!BestComponent || ZOrder >= BestZOrder)
				{
					BestComponent = SceneComponent;
					BestZOrder = ZOrder;
				}
			}
		}

		return BestComponent;
	}

	bool IsUIScreenTransformableComponent(const USceneComponent* Component)
	{
		return Cast<UUIImageComponent>(Component) || Cast<UUIScreenTextComponent>(Component);
	}

	bool GetUIScreenTextBounds(const UUIScreenTextComponent* TextComponent, float& OutX, float& OutY, float& OutWidth, float& OutHeight)
	{
		return TextComponent && TextComponent->GetResolvedScreenBounds(OutX, OutY, OutWidth, OutHeight);
	}

	template <typename Func>
	bool ApplyToUIComponent(const USceneComponent* Component, Func&& Action)
	{
		if (const UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(Component))
		{
			return Action(ImageComponent);
		}

		if (const UUIScreenTextComponent* TextComponent = Cast<UUIScreenTextComponent>(Component))
		{
			return Action(TextComponent);
		}

		return false;
	}

	template <typename Func>
	bool ApplyToMutableUIComponent(USceneComponent* Component, Func&& Action)
	{
		if (UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(Component))
		{
			return Action(ImageComponent);
		}

		if (UUIScreenTextComponent* TextComponent = Cast<UUIScreenTextComponent>(Component))
		{
			return Action(TextComponent);
		}

		return false;
	}

	bool GetUIScreenComponentBounds(const USceneComponent* Component, float& OutX, float& OutY, float& OutWidth, float& OutHeight)
	{
		return ApplyToUIComponent(Component, [&](const auto* UIComponent) {
			using TComponent = std::remove_cv_t<std::remove_pointer_t<decltype(UIComponent)>>;
			if constexpr (std::is_same_v<TComponent, UUIImageComponent>)
			{
				const FVector2 ResolvedPosition = UIComponent->GetResolvedScreenPosition();
				const FVector2 ResolvedSize = UIComponent->GetResolvedScreenSize();
				OutX = ResolvedPosition.X;
				OutY = ResolvedPosition.Y;
				OutWidth = (std::max)(1.0f, ResolvedSize.X);
				OutHeight = (std::max)(1.0f, ResolvedSize.Y);
				return true;
			}
			else
			{
				return GetUIScreenTextBounds(UIComponent, OutX, OutY, OutWidth, OutHeight);
			}
		});
	}

	bool GetUIScreenComponentPosition(const USceneComponent* Component, FVector& OutPosition)
	{
		return ApplyToUIComponent(Component, [&](const auto* UIComponent) {
			using TComponent = std::remove_cv_t<std::remove_pointer_t<decltype(UIComponent)>>;
			if constexpr (std::is_same_v<TComponent, UUIImageComponent>)
			{
				const FVector2 ResolvedPosition = UIComponent->GetResolvedScreenPosition();
				OutPosition = FVector(ResolvedPosition.X, ResolvedPosition.Y, UIComponent->GetScreenPosition().Z);
				return true;
			}
			else
			{
				float X = 0.0f;
				float Y = 0.0f;
				float Width = 0.0f;
				float Height = 0.0f;
				if (!UIComponent->GetResolvedScreenBounds(X, Y, Width, Height))
				{
					return false;
				}

				OutPosition = FVector(X, Y, UIComponent->GetScreenPosition().Z);
				return true;
			}
		});
	}

	bool SetUIScreenComponentPosition(USceneComponent* Component, const FVector& InPosition)
	{
		return ApplyToMutableUIComponent(Component, [&](auto* UIComponent) {
			using TComponent = std::remove_cv_t<std::remove_pointer_t<decltype(UIComponent)>>;
			if (UIComponent->IsAnchoredLayoutEnabled())
			{
				float CurrentX = 0.0f;
				float CurrentY = 0.0f;
				if constexpr (std::is_same_v<TComponent, UUIImageComponent>)
				{
					const FVector2 CurrentResolvedPosition = UIComponent->GetResolvedScreenPosition();
					CurrentX = CurrentResolvedPosition.X;
					CurrentY = CurrentResolvedPosition.Y;
				}
				else
				{
					float CurrentWidth = 0.0f;
					float CurrentHeight = 0.0f;
					if (!UIComponent->GetResolvedScreenBounds(CurrentX, CurrentY, CurrentWidth, CurrentHeight))
					{
						return false;
					}
				}

				FVector AnchorOffset = UIComponent->GetAnchorOffset();
				AnchorOffset.X += InPosition.X - CurrentX;
				AnchorOffset.Y += InPosition.Y - CurrentY;
				UIComponent->SetAnchorOffset(AnchorOffset);
				return true;
			}

			UIComponent->SetScreenPosition(InPosition);
			return true;
		});
	}

	bool ConvertMouseToViewportPixelUnclamped(const ImVec2& Pos, const FRect& Rect, const FViewport* Viewport, float FallbackW, float FallbackH, float& OutX, float& OutY)
	{
		if (Rect.Width <= 0.0f || Rect.Height <= 0.0f)
		{
			return false;
		}

		const float LocalX = Pos.x - Rect.X;
		const float LocalY = Pos.y - Rect.Y;
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

	bool TryConvertMouseToViewportPixel(const ImVec2& Pos, const FRect& Rect, const FViewport* Viewport, float FallbackW, float FallbackH, float& OutX, float& OutY)
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

		return ConvertMouseToViewportPixelUnclamped(Pos, Rect, Viewport, FallbackW, FallbackH, OutX, OutY);
	}

	bool TryConvertPixelToScreen(float PX, float PY, const FRect& ScreenRect, const FViewport* Viewport, float FallbackW, float FallbackH, float& OutX, float& OutY)
	{
		if (ScreenRect.Width <= 0.0f || ScreenRect.Height <= 0.0f)
		{
			return false;
		}

		const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : FallbackW;
		const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : FallbackH;
		if (TargetWidth <= 0.0f || TargetHeight <= 0.0f)
		{
			return false;
		}

		const float ScaleX = ScreenRect.Width / TargetWidth;
		const float ScaleY = ScreenRect.Height / TargetHeight;
		OutX = ScreenRect.X + PX * ScaleX;
		OutY = ScreenRect.Y + PY * ScaleY;
		return true;
	}

	bool ProjectWorldToViewport(const FMatrix& ViewProjection, const FVector& WorldPosition, float ViewportWidth, float ViewportHeight, float& OutScreenX, float& OutScreenY, float& OutDepth)
	{
		const FVector ClipSpace = ViewProjection.TransformPositionWithW(WorldPosition);
		OutScreenX = (ClipSpace.X * 0.5f + 0.5f) * ViewportWidth;
		OutScreenY = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * ViewportHeight;
		OutDepth = ClipSpace.Z;
		return std::isfinite(OutScreenX) && std::isfinite(OutScreenY) && std::isfinite(OutDepth);
	}

	bool EditorRaycastAllVisiblePrimitives(UWorld* World, const FRay& Ray, FRayHitResult& OutHitResult, AActor*& OutActor)
	{
		FRayHitResult BestHit{};
		AActor* BestActor = nullptr;
		if (!World)
		{
			return false;
		}

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !Actor->IsVisible())
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
				if (!Primitive || !Primitive->IsVisible())
				{
					continue;
				}

				FRayHitResult CandidateHit{};
				if (!Primitive->LineTraceComponent(Ray, CandidateHit))
				{
					float AABBTMin = 0.0f;
					float AABBTMax = 0.0f;
					const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
					if (!Bounds.IsValid() || !FRayUtils::IntersectRayAABB(Ray, Bounds.Min, Bounds.Max, AABBTMin, AABBTMax))
					{
						continue;
					}

					CandidateHit.HitComponent = Primitive;
					CandidateHit.Distance = AABBTMin >= 0.0f ? AABBTMin : AABBTMax;
					CandidateHit.WorldHitLocation = Ray.Origin + Ray.Direction * CandidateHit.Distance;
					CandidateHit.bHit = true;
				}

				if (CandidateHit.Distance < BestHit.Distance)
				{
					BestHit = CandidateHit;
					BestActor = Actor;
				}
			}
		}

		if (!BestActor)
		{
			return false;
		}

		OutHitResult = BestHit;
		OutActor = BestActor;
		return true;
	}

	void BuildBoundingBoxCorners(const FBoundingBox& Bounds, FVector OutCorners[8])
	{
		const FVector& Min = Bounds.Min;
		const FVector& Max = Bounds.Max;
		OutCorners[0] = FVector(Min.X, Min.Y, Min.Z);
		OutCorners[1] = FVector(Max.X, Min.Y, Min.Z);
		OutCorners[2] = FVector(Min.X, Max.Y, Min.Z);
		OutCorners[3] = FVector(Max.X, Max.Y, Min.Z);
		OutCorners[4] = FVector(Min.X, Min.Y, Max.Z);
		OutCorners[5] = FVector(Max.X, Min.Y, Max.Z);
		OutCorners[6] = FVector(Min.X, Max.Y, Max.Z);
		OutCorners[7] = FVector(Max.X, Max.Y, Max.Z);
	}

	AActor* FindScreenSpacePrimitiveAt(UWorld* World, const UCameraComponent* Camera, float MouseX, float MouseY, float ViewportW, float ViewportH, UPrimitiveComponent*& OutPrimitive)
	{
		OutPrimitive = nullptr;
		if (!World || !Camera || ViewportW <= 0.0f || ViewportH <= 0.0f)
		{
			return nullptr;
		}

		const FMatrix ViewProjection = Camera->GetViewProjectionMatrix();
		AActor* BestActor = nullptr;
		UPrimitiveComponent* BestPrimitive = nullptr;
		float BestArea = FLT_MAX;
		float BestDepth = FLT_MAX;
		float BestScore = FLT_MAX;
		constexpr float ScreenPickPadding = 12.0f;
		constexpr float CenterPickRadiusSq = 24.0f * 24.0f;

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !Actor->IsVisible())
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
				if (!Primitive || !Primitive->IsVisible())
				{
					continue;
				}

				const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
				if (!Bounds.IsValid())
				{
					continue;
				}

				FVector Corners[8];
				BuildBoundingBoxCorners(Bounds, Corners);
				float MinX = FLT_MAX;
				float MinY = FLT_MAX;
				float MaxX = -FLT_MAX;
				float MaxY = -FLT_MAX;
				float MinDepth = FLT_MAX;
				bool bProjectedAny = false;
				for (const FVector& Corner : Corners)
				{
					float ScreenX = 0.0f;
					float ScreenY = 0.0f;
					float Depth = 0.0f;
					if (!ProjectWorldToViewport(ViewProjection, Corner, ViewportW, ViewportH, ScreenX, ScreenY, Depth))
					{
						continue;
					}

					MinX = (std::min)(MinX, ScreenX);
					MinY = (std::min)(MinY, ScreenY);
					MaxX = (std::max)(MaxX, ScreenX);
					MaxY = (std::max)(MaxY, ScreenY);
					MinDepth = (std::min)(MinDepth, Depth);
					bProjectedAny = true;
				}

				if (!bProjectedAny)
				{
					continue;
				}

				const float ExpandedMinX = MinX - ScreenPickPadding;
				const float ExpandedMaxX = MaxX + ScreenPickPadding;
				const float ExpandedMinY = MinY - ScreenPickPadding;
				const float ExpandedMaxY = MaxY + ScreenPickPadding;
				if (MouseX < ExpandedMinX || MouseX > ExpandedMaxX || MouseY < ExpandedMinY || MouseY > ExpandedMaxY)
				{
					continue;
				}

				const float Area = (std::max)(1.0f, MaxX - MinX) * (std::max)(1.0f, MaxY - MinY);
				const float ClampedX = (std::max)(MinX, (std::min)(MouseX, MaxX));
				const float ClampedY = (std::max)(MinY, (std::min)(MouseY, MaxY));
				const float DistanceSq = (MouseX - ClampedX) * (MouseX - ClampedX)
					+ (MouseY - ClampedY) * (MouseY - ClampedY);
				if (!BestActor
					|| DistanceSq < BestScore
					|| (std::abs(DistanceSq - BestScore) < 1.0f && (Area < BestArea || (std::abs(Area - BestArea) < 1.0f && MinDepth < BestDepth))))
				{
					BestActor = Actor;
					BestPrimitive = Primitive;
					BestScore = DistanceSq;
					BestArea = Area;
					BestDepth = MinDepth;
				}
			}
		}

		if (!BestActor)
		{
			for (AActor* Actor : World->GetActors())
			{
				if (!Actor || !Actor->IsVisible())
				{
					continue;
				}

				float ScreenX = 0.0f;
				float ScreenY = 0.0f;
				float Depth = 0.0f;
				if (!ProjectWorldToViewport(ViewProjection, Actor->GetActorLocation(), ViewportW, ViewportH, ScreenX, ScreenY, Depth))
				{
					continue;
				}

				const float DistanceSq = (MouseX - ScreenX) * (MouseX - ScreenX)
					+ (MouseY - ScreenY) * (MouseY - ScreenY);
				if (DistanceSq > CenterPickRadiusSq)
				{
					continue;
				}

				BestActor = Actor;
				for (UActorComponent* Component : Actor->GetComponents())
				{
					UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
					if (Primitive && Primitive->IsVisible())
					{
						BestPrimitive = Primitive;
						break;
					}
				}
				break;
			}
		}

		OutPrimitive = BestPrimitive;
		return BestActor;
	}

	FVector FindClosestVertex(UWorld* World, const FRay& Ray, float MaxDistance)
	{
		if (!World)
		{
			return FVector::ZeroVector;
		}

		FVector BestVertex = FVector::ZeroVector;
		float BestDistSq = MaxDistance * MaxDistance;
		bool bFound = false;
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !Actor->IsVisible() || Actor->IsA<UGizmoComponent>())
			{
				continue;
			}

			for (UActorComponent* Comp : Actor->GetComponents())
			{
				UMeshComponent* MeshComp = Comp->IsA<UMeshComponent>() ? static_cast<UMeshComponent*>(Comp) : nullptr;
				if (!MeshComp)
				{
					continue;
				}

				FMeshDataView View = MeshComp->GetMeshDataView();
				if (!View.IsValid())
				{
					continue;
				}

				const FMatrix WorldMat = MeshComp->GetWorldMatrix();
				for (uint32 i = 0; i < View.VertexCount; ++i)
				{
					const FVector LocalPos = View.GetPosition(i);
					const FVector WorldPos = WorldMat.TransformPositionWithW(LocalPos);
					const FVector W = WorldPos - Ray.Origin;
					const float Proj = W.Dot(Ray.Direction);
					const FVector ClosestP = Ray.Origin + Ray.Direction * (std::max)(0.0f, Proj);
					const float DistSq = (WorldPos - ClosestP).Dot(WorldPos - ClosestP);
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						BestVertex = WorldPos;
						bFound = true;
					}
				}
			}
		}

		return bFound ? BestVertex : FVector::ZeroVector;
	}
}
