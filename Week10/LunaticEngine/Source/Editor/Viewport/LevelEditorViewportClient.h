#pragma once

#include "Editor/Viewport/EditorViewportClient.h"

#include "Core/RayTypes.h"
#include "UI/SWindow.h"
#include "imgui.h"

class AActor;
class FOverlayStatSystem;
class FSelectionManager;
class UGizmoComponent;
class ULightComponentBase;
class UWorld;

// UE의 FLevelEditorViewportClient 대응.
// 레벨 월드 편집에 필요한 선택, 기즈모, PIE, 라이트 뷰, 레벨 전용 단축키를 담당합니다.
class FLevelEditorViewportClient : public FEditorViewportClient
{
public:
	FLevelEditorViewportClient();
	~FLevelEditorViewportClient() override;

	void SetOverlayStatSystem(FOverlayStatSystem* InOverlayStatSystem) { OverlayStatSystem = InOverlayStatSystem; }
	UWorld* GetWorld() const;

	void SetGizmo(UGizmoComponent* InGizmo) { Gizmo = InGizmo; }
	UGizmoComponent* GetGizmo() { return Gizmo; }

	void SetSelectionManager(FSelectionManager* InSelectionManager) { SelectionManager = InSelectionManager; }

	void SetViewportType(ELevelViewportType NewType);
	bool FocusActor(AActor* Actor);

	void Tick(float DeltaTime) override;
	bool HandleInputSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime) override;

	void SetLayoutWindow(SWindow* InWindow) { LayoutWindow = InWindow; }
	SWindow* GetLayoutWindow() const { return LayoutWindow; }
	void UpdateLayoutRect();
	void RenderViewportImage(bool bIsActiveViewport);

	void SetLightViewOverride(ULightComponentBase* Light);
	void ClearLightViewOverride();
	bool IsViewingFromLight() const { return LightViewOverride != nullptr; }
	ULightComponentBase* GetLightViewOverride() const { return LightViewOverride; }

	int32 GetPointLightFaceIndex() const { return PointLightFaceIndex; }
	void SetPointLightFaceIndex(int32 Index) { PointLightFaceIndex = (Index < 0) ? 0 : (Index > 5) ? 5 : Index; }

protected:
	bool CanProcessCameraInput() const override;

private:
	void SetupInput();
	void ReleaseLevelInput();

	void OnOrbit(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnFocus(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnDelete(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnDuplicate(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnToggleGizmoMode(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnToggleCoordSystem(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnEscape(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);
	void OnTogglePIE(const FInputActionValue& Value, const FInputSystemSnapshot& Snapshot);

	void TickEditorShortcuts(const FInputSystemSnapshot& Snapshot);
	void TickInteraction(const FInputSystemSnapshot& Snapshot, float DeltaTime);
	void HandleDragStart(const FInputSystemSnapshot& Snapshot, const FRay& Ray);
	void ProcessMarqueeSelection(bool bAppendSelection);

	void DrawUIScreenTranslateGizmo();
	bool HasUIScreenTranslateGizmo() const;
	int32 HitTestUIScreenTranslateGizmo(const ImVec2& MousePos) const;
	bool BeginUIScreenTranslateDrag(const ImVec2& MousePos);
	void UpdateUIScreenTranslateDrag(const ImVec2& MousePos);
	void EndUIScreenTranslateDrag(bool bCommitChange);

private:
	SWindow* LayoutWindow = nullptr;
	FOverlayStatSystem* OverlayStatSystem = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	FSelectionManager* SelectionManager = nullptr;
	ULightComponentBase* LightViewOverride = nullptr;
	int32 PointLightFaceIndex = 0;

	bool bIsMarqueeSelecting = false;
	FVector MarqueeStartPos;
	FVector MarqueeCurrentPos;

	FInputMappingContext* LevelEditorMappingContext = nullptr;
	TArray<FInputAction*> EditorInputActions;

	FInputAction* ActionEditorOrbit = nullptr;
	FInputAction* ActionEditorFocus = nullptr;
	FInputAction* ActionEditorDelete = nullptr;
	FInputAction* ActionEditorDuplicate = nullptr;
	FInputAction* ActionEditorToggleGizmoMode = nullptr;
	FInputAction* ActionEditorToggleCoordSystem = nullptr;
	FInputAction* ActionEditorEscape = nullptr;
	FInputAction* ActionEditorTogglePIE = nullptr;
	FInputAction* ActionEditorDecreaseSnap = nullptr;
	FInputAction* ActionEditorIncreaseSnap = nullptr;
	FInputAction* ActionEditorVertexSnap = nullptr;
	FInputAction* ActionEditorSnapToFloor = nullptr;
	FInputAction* ActionEditorSetBookmark = nullptr;
	FInputAction* ActionEditorJumpToBookmark = nullptr;
	FInputAction* ActionEditorSetViewportPerspective = nullptr;
	FInputAction* ActionEditorSetViewportTop = nullptr;
	FInputAction* ActionEditorSetViewportFront = nullptr;
	FInputAction* ActionEditorSetViewportRight = nullptr;
	FInputAction* ActionEditorToggleGridSnap = nullptr;
	FInputAction* ActionEditorToggleRotationSnap = nullptr;
	FInputAction* ActionEditorToggleScaleSnap = nullptr;
	FInputAction* ActionEditorToggleGameView = nullptr;

	int32 HoveredUIScreenGizmoAxis = 0;
	int32 ActiveUIScreenGizmoAxis = 0;
	bool bDraggingUIScreenGizmo = false;
	ImVec2 LastUIScreenGizmoMousePos = ImVec2(0.0f, 0.0f);
};
