#pragma once

#include "Engine/Source/Runtime/Editor/Public/BaseTransformGizmo.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/RingComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

class APivotTransformGizmo : public ABaseTransformGizmo
{
  public:
    APivotTransformGizmo();
    virtual ~APivotTransformGizmo() override;

    virtual void Render(URenderer &renderer, const FMatrix<float> &ViewMatrix) override;

    virtual bool OnMouseDown(const FVector<float> &RayOrigin, const FVector<float> &RayDir) override;
    virtual void OnMouseMove(const FVector<float> &RayOrigin, const FVector<float> &RayDir) override;
    virtual void OnMouseHover(const FVector<float> &RayOrigin, const FVector<float> &RayDir) override;
    virtual void OnMouseUp() override;

    void ToggleMode();
    void UpdateColor();

  private:
    float          InitialRayDistance = 0.0f; // 카메라에서 쏜 Ray가 기즈모 축에 부딪힌 깊이(거리)
    FTransform     InitialObjectTransform;    // (드래그로 객체를 변경하기 위한) 객체의 초기 Transform
    FVector<float> GizmoPlaneNormal;
    FVector<float> InitialDragVector;

    EGizmoAxis HoveredAxis = EGizmoAxis::None;

    TArray<UPrimitiveComponent *> TranslateGizmoComponents;
    TArray<UPrimitiveComponent *> RotateGizmoComponents;
    TArray<UPrimitiveComponent *> ScaleGizmoComponents;
};