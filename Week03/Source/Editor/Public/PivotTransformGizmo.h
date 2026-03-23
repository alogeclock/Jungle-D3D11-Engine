#pragma once

#include "Source/Editor/Public/BaseTransformGizmo.h"
#include "Source/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Source/Engine/Public/Classes/Components/RingComponent.h"
#include "Source/Engine/Public/Classes/Components/CubeArrowComponent.h"

// [추가] 다중 선택 시 개별 객체의 초기 상태를 기억하기 위한 구조체
struct FSelectedObjectState
{
    USceneComponent* Component;
    FTransform InitialWorldTransform;
};

class APivotTransformGizmo : public ABaseTransformGizmo
{
  public:
    APivotTransformGizmo(const FString &InString);
    virtual ~APivotTransformGizmo() override;

    virtual void Tick(float DeltaTime) override;

    virtual bool OnMouseDown(const FVector<float> &RayOrigin, const FVector<float> &RayDir) override;
    virtual void OnMouseMove(const FVector<float> &RayOrigin, const FVector<float> &RayDir) override;
    virtual void OnMouseHover(const FVector<float> &RayOrigin, const FVector<float> &RayDir) override;
    virtual void OnMouseUp() override;
    virtual bool OnKeyDown(const FKey& Key);

    void ToggleMode();
    void UpdateColor();

    FTransform CalculateUnscaledTransform(FTransform TargetTransform);    
    
    // [변경] 단일 Anchor 대신 타겟 컴포넌트 전체를 가져오고, 중심점(Center) Transform을 구하는 함수로 대체
    void GetTargetComponents(TArray<USceneComponent*>& OutComponents);
    bool GetGizmoCenterTransform(FTransform& OutTransform);

  private:
    float          InitialRayDistance = 0.0f; 
    FVector<float> GizmoPlaneNormal;
    FVector<float> InitialDragVector;

    // [변경] 기존 단일 객체 Transform 대신 기즈모 중심 Transform과 다중 객체 배열 사용
    FTransform InitialGizmoTransform;    
    TArray<FSelectedObjectState> DraggingObjects;

    void UpdateVisibility();

    EGizmoAxis HoveredAxis = EGizmoAxis::None;

    TArray<UPrimitiveComponent *> TranslateGizmoComponents;
    TArray<UPrimitiveComponent *> RotateGizmoComponents;
    TArray<UPrimitiveComponent *> ScaleGizmoComponents;

};