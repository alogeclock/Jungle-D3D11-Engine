#pragma once

#include "Source/Editor/Public/Application.h"
#include "Source/Core/Public/Math/Matrix.h"
#include "Source/Core/Public/Math/Vector.h"
#include "Source/Editor/Public/EditorEngine.h"

// 사용자의 카메라 관리

class FViewport;

struct FRay
{
    FVector<float> Origin;
    FVector<float> Direction;
    FRay(const FVector<float>& InOrigin, const FVector<float>& InDirection)
        : Origin(InOrigin), Direction(InDirection) {}
};

/**
 * Stores the transformation data for the viewport camera
 */
struct FViewportCameraTransform
{
  public:
    FViewportCameraTransform();

    /** Sets the transform's location */
    void SetLocation(const FVector<float> &Position);

    /** Sets the transform's rotation */
    void SetRotation(const FVector<float> &Rotation) { ViewRotation = Rotation; }

    /** Sets the location to look at during orbit */
    void SetLookAt(const FVector<float> &InLookAt) { LookAt = InLookAt; }

    /** Sets the FOV for imGuit */
    void SetFOV(float radian) { FOV = radian; }

    /** Set the ortho zoom amount */
    void SetOrthoZoom(float InOrthoZoom)
    {
        if (InOrthoZoom >= Max_OrthoZoom && InOrthoZoom <= Min_OrthoZoom)
            return;

        OrthoZoom = InOrthoZoom;
    }

    void SetMaxLocation(double InMaxLocation) { MaxLocation = InMaxLocation; }

    /** @return The transform's location */
    inline FVector<float> &GetLocation() { return ViewLocation; }

    /** @return The transform's rotation */
    inline FVector<float> &GetRotation() { return ViewRotation; }

    /** @return The look at point for orbiting */
    inline const FVector<float> &GetLookAt() const { return LookAt; }

    /** @return The transform's FOV */
    inline float &GetFOV() { return FOV; }

    /** @return The ortho zoom amount */
    inline float GetOrthoZoom() const { return OrthoZoom; }

    /**
     * Computes a matrix to use for viewport location and rotation when orbiting
     */
    FMatrix<float> ComputeOrbitMatrix() const;

  private:
    /** Current viewport Position. */
    FVector<float> ViewLocation;
    /** Current Viewport orientation; valid only for perspective projections. */
    FVector<float> ViewRotation;
    /** Desired viewport location when animating between two locations */
    FVector<float> DesiredLocation;
    /** When orbiting, the point we are looking at */
    FVector<float> LookAt;
    /** Viewport start location when animating to another location */
    FVector<float> StartLocation;
    /** FOV */
    float FOV = 3.14159265f / 2.0f;
    /** Ortho zoom amount */
    float OrthoZoom = 1.0f;
    float Max_OrthoZoom = 1000.0f;
    float Min_OrthoZoom = 1.0f;
    /** Location is clamped to a box around the origin with this radius */
    double MaxLocation = 1000.0f;
};

class FEditorViewportClient
{
  public:
    FEditorViewportClient(FViewport *viewport);
    ~FEditorViewportClient();

  public:
    // FViewport → 매 프레임 호출
    void Tick(float DeltaTime, FViewport *Viewport);

    bool InputKey(const FInputEventState &InputState);
    void MouseMove(FViewport *Viewport, int32 X, int32 Y);
    void InputAxis(FViewport *Viewport, FKey Key, float Delta, float DeltaTime);

    // 현재 카메라 View Matrix를 Object에 전달하여 행렬곱
    FMatrix<float> GetViewMatrix() const;
    FMatrix<float> GetProjectionMatrix(float width, float height);
    float         *GetMoveSpeedPtr() { return &MoveSpeed; }
    float         *GetRotSpeedPtr() { return &RotSpeed; }
    
    float GetGridStep() const { return GridStep; }
    void SetGridStepAndUpdate(float InGridStep) { GridStep = InGridStep; }

    // 기즈모 및 메인 축 렌더링 함수
    void Render(URenderer &renderer);

    // 카메라 트랜스폼 직접 접근 (필요 시)
    const FViewportCameraTransform &GetCameraTransform() const { return CameraTransform; }

    bool GetDrawAABB() const { return bDrawAABB; }
    void SetDrawAABB(bool bInDraw) { bDrawAABB = bInDraw; }

    EViewModeIndex GetViewMode() const { return ViewMode; }
    void SetViewMode(EViewModeIndex InViewMode) { ViewMode = InViewMode; }
    
    EEngineShowFlags GetShowFlags() const { return ShowFlags; }
    void SetShowFlags(EEngineShowFlags InShowFlags) { ShowFlags = InShowFlags; }

    void LoadConfig();
    void SaveConfig();

    void SetGrid(AGrid* InGrid);
    void SetAxis(AAxis* InAxis);
    void SetGridStep(float InGridStep);
    void SetGizmo(APivotTransformGizmo* InGizmo) { Gizmo = InGizmo; }

  private:
    // WASD 이동 누적
    void ApplyMovement(float DeltaTime, FViewport *Viewport);

    // Ray
    FRay GetPickingRay();
    void PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

    // Viewport
    FViewport *Viewport = nullptr;

    // 카메라
    FViewportCameraTransform CameraTransform;

    float                  MoveSpeed = 5.0f; // units/sec
    float                  RotSpeed = 0.1f;  // deg/pixel
    float                  GridStep = 1.0f;
    static constexpr float ZoomSpeed = 10.0f;

    bool  bLeftMouseDragging = false;
    bool  bRightMouseDragging = false;
    int32 LastMouseX = 0;
    int32 LastMouseY = 0;

    APivotTransformGizmo *Gizmo = nullptr;
    AAxis                *Axis = nullptr;
    AGrid                *Grid = nullptr;

    bool bDrawAABB = true;
    EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit;
    EEngineShowFlags ShowFlags = EEngineShowFlags::SF_Primitives | EEngineShowFlags::SF_UUID;
};
