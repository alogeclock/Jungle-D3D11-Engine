#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/BaseTransformGizmo.h"

ABaseTransformGizmo::ABaseTransformGizmo() : GizmoType(EGizmoHandleType::Translate), ActiveAxis(EGizmoAxis::None), bIsDragging(false), TargetObject(nullptr) {}

ABaseTransformGizmo::~ABaseTransformGizmo() {}

void ABaseTransformGizmo::SetTargetObject(UPrimitiveComponent *InTarget) { TargetObject = InTarget; }

UPrimitiveComponent *ABaseTransformGizmo::GetTargetObject() const { return TargetObject; }

// Ray는 카메라에서 마우스 커서 방향으로 발사되는 벡터이며, Axis는 현재 Gizmo에서 결정된 축 방향 벡터이다.
// OutAxisT 변수는 Axis 방향으로 마우스가 이동한 거리를 나타내고, OutRayT 변수는 Ray 방향으로 마우스가 이동한 거리를 나타낸다.
// 이 함수는 Ray와 Axis(x, y, z축)가 가장 가까워지는 거리를 계산하여, 사용자가 현재 마우스로 클릭한 축을 찾는다.
float ABaseTransformGizmo::CalculateDistanceToAxis(const FVector<float> &RayOrigin, const FVector<float> &RayDir, const FVector<float> &AxisOrigin,
                                                    const FVector<float> &AxisDir, float &OutAxisT, float &OutRayT)
{
    FVector<float> W0 = RayOrigin - AxisOrigin;

    float a = FVector<float>::DotProduct(RayDir, RayDir);   // RayDir의 제곱 길이
    float b = FVector<float>::DotProduct(RayDir, AxisDir);  // RayDir와 AxisDir의 내적
    float c = FVector<float>::DotProduct(AxisDir, AxisDir); // AxisDir의 제곱 길이 (일반적으로 1)
    float d = FVector<float>::DotProduct(RayDir, W0);       // RayDir와 RayOrigin-AxisOrigin 벡터의 내적
    float e = FVector<float>::DotProduct(AxisDir, W0);      // AxisDir와 RayOrigin-AxisOrigin 벡터의 내적

    float Denominator = a * c - b * b;

    // 평행한 경우
    if (Denominator < 0.0001f)
    {
        OutRayT = 0.0f;
        OutAxisT = e / c;
        FVector<float> Diff = W0 - (AxisDir * OutAxisT);
        return Diff.Size();
    }

    OutRayT = (b * e - c * d) / Denominator;
    OutAxisT = (a * e - b * d) / Denominator;

    // 최단 거리 지점에서의 벡터 차이
    FVector<float> PointOnRay = RayOrigin + (RayDir * OutRayT);
    FVector<float> PointOnAxis = AxisOrigin + (AxisDir * OutAxisT);

    FVector<float> ClosestDiff = PointOnRay - PointOnAxis;
    return ClosestDiff.Size(); // 선분 간의 최단 거리 반환
}