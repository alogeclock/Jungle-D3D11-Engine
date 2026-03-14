#pragma once

#include "Engine/Source/Runtime/Editor/Public/BaseTransformGizmo.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"

class APivotTransformGizmo : public ABaseTransformGizmo
{
  public:
    APivotTransformGizmo();
    virtual ~APivotTransformGizmo() override;

    virtual void Render(URenderer &Renderer) override;

    virtual bool OnMouseDown(const FVector<float> &RayOrigin, const FVector<float> &RayDir) override;
    virtual void OnMouseMove(const FVector<float> &RayOrigin, const FVector<float> &RayDir) override;
    virtual void OnMouseUp() override;

  private:
    UAxisComponent *AxisComponent;

    // 드래그 계산용 임시 데이터
    float          InitialIntersectionT;  // 드래그 시작 시점의 Ray T값
    FTransform     InitialObjectTransform; // 드래그 시작 시 객체의 Transform

    // 두 3D 직선(Ray와 기즈모 축) 간의 최단 거리를 계산하는 헬퍼 함수
    float CalculateDistanceToAxis(const FVector<float> &RayOrigin, const FVector<float> &RayDir, const FVector<float> &AxisOrigin,
                                  const FVector<float> &AxisDir, float &OutAxisT, float &OutRayT);
};