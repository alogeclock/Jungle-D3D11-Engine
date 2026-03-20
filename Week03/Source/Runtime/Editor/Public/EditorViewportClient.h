#pragma once

#include "Source/Runtime/Editor/Public/Application.h"
#include "Source/Runtime/Core/Public/Math/Matrix.h"
#include "Source/Runtime/Core/Public/Math/Vector.h"
#include "Source/Runtime/Engine/Public/ImGuiManager.h"

// 사용자의 Input 이벤트, 카메라 관리

// TODO: WSAD, 우클릭 카메라 조정

class FViewport;

struct FKey
{
    uint32 KeyCode;
    explicit FKey(uint32 InCode) : KeyCode(InCode) {}
    bool operator==(const FKey &O) const { return KeyCode == O.KeyCode; }
};

namespace EKeys
{
    inline const FKey W{'W'};
    inline const FKey A{'A'};
    inline const FKey S{'S'};
    inline const FKey D{'D'};
    inline const FKey Q{'Q'};
    inline const FKey E{'E'};
    inline const FKey Space{VK_SPACE};
    inline const FKey LeftMouseButton{VK_LBUTTON};
    inline const FKey RightMouseButton{VK_RBUTTON};
} // namespace EKeys

enum class EInputEvent : uint8
{
    Pressed = 0,
    Released = 1,
    Repeat = 2,
    Axis = 3,
};

struct FInputEventState
{
  public:
    FInputEventState(FViewport *InViewport, FKey InKey, EInputEvent InInputEvent) : Viewport(InViewport), Key(InKey), InputEvent(InInputEvent) {}

    FViewport  *GetViewport() const { return Viewport; }
    EInputEvent GetInputEvent() const { return InputEvent; }
    FKey        GetKey() const { return Key; }

    bool IsLeftMouseButtonPressed() const;
    bool IsRightMouseButtonPressed() const;
    bool IsButtonPressed(FKey InKey) const;

  private:
    /** Viewport the event was sent to */
    FViewport *Viewport;
    /** Pressed Key */
    FKey Key;
    /** Key event */
    EInputEvent InputEvent;
};

struct FRay
{
    FVector<float> Origin;
    FVector<float> Direction;
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

    // 현재 카메라 View Matrix ? Object에 전달하여 행렬곱
    FMatrix<float> GetViewMatrix() const;
    FMatrix<float> GetProjectionMatrix(float width, float height);

    // 기즈모 및 메인 축 렌더링 함수
    void Render(URenderer &renderer);

    // 카메라 트랜스폼 직접 접근 (필요 시)
    const FViewportCameraTransform &GetCameraTransform() const { return CameraTransform; }

  private:
    // WASD 이동 누적
    void ApplyMovement(float DeltaTime, FViewport *Viewport);

    // Ray
    FRay GetPickingRay();
    void PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection) const;

    // Viewport
    FViewport *Viewport = nullptr;

    // 카메라
    FViewportCameraTransform CameraTransform;

    static constexpr float MoveSpeed = 5.0f; // units/sec
    static constexpr float RotSpeed = 0.1f;  // deg/pixel
    static constexpr float ZoomSpeed = 10.0f;

    bool  bLeftMouseDragging = false;
    bool  bRightMouseDragging = false;
    int32 LastMouseX = 0;
    int32 LastMouseY = 0;

    APivotTransformGizmo *Gizmo = nullptr;
    AAxis                *Axis = nullptr;
    AGrid                *Grid = nullptr;
};
