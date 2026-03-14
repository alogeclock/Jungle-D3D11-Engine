#include "Engine/Source/Runtime/Editor/Public/PivotTransformGizmo.h"

APivotTransformGizmo::APivotTransformGizmo() { AxisComponent = new UAxisComponent(); }

APivotTransformGizmo::~APivotTransformGizmo()
{
    if (AxisComponent)
    {
        delete AxisComponent;
    }
}

void APivotTransformGizmo::Render(URenderer &Renderer)
{
    if (TargetObject == nullptr || AxisComponent == nullptr)
        return;

    // 기즈모는 항상 선택된 객체의 위치에 렌더링
    FConstants GizmoConstants;
    GizmoConstants.worldMatrix = TargetObject->GetTransform().ToMatrix();
    Renderer.UpdateConstant(GizmoConstants);

    // Depth Test를 끈 상태로 렌더링한 뒤 다시 켜준다.
    Renderer.SetDepthTestEnable(false);
    AxisComponent->RenderPrimitive(Renderer);
    Renderer.SetDepthTestEnable(true);
}

bool APivotTransformGizmo::OnMouseDown(const FVector<float> &RayOrigin, const FVector<float> &RayDir)
{
    if (TargetObject == nullptr)
        return false;

    // 드래그 시작 시점의 전체 Transform 정보를 저장
    InitialObjectTransform = TargetObject->GetTransform();
    FVector<float> GizmoOrigin = TargetObject->GetTransform().Location;

    // 기즈모를 선택하기 위한 허용 오차 (클릭 범위)
    const float SelectThreshold = 0.5f;

    // 각 축(X, Y, Z) 방향 벡터
    FVector<float> AxisDirX(1.0f, 0.0f, 0.0f);
    FVector<float> AxisDirY(0.0f, 1.0f, 0.0f);
    FVector<float> AxisDirZ(0.0f, 0.0f, 1.0f);

    float DistX, DistY, DistZ;
    float AxisTX, AxisTY, AxisTZ;
    float RayTX, RayTY, RayTZ;

    DistX = CalculateDistanceToAxis(RayOrigin, RayDir, GizmoOrigin, AxisDirX, AxisTX, RayTX);
    DistY = CalculateDistanceToAxis(RayOrigin, RayDir, GizmoOrigin, AxisDirY, AxisTY, RayTY);
    DistZ = CalculateDistanceToAxis(RayOrigin, RayDir, GizmoOrigin, AxisDirZ, AxisTZ, RayTZ);

    // 양수 방향(축의 앞쪽)만 클릭되도록 하고, 가장 가까운 축을 찾는다.
    ActiveAxis = EGizmoAxis::None;
    float MinDist = SelectThreshold;

    if (DistX < MinDist && AxisTX > 0.0f)
    {
        MinDist = DistX;
        ActiveAxis = EGizmoAxis::X;
        InitialIntersectionT = AxisTX;
    }
    if (DistY < MinDist && AxisTY > 0.0f)
    {
        MinDist = DistY;
        ActiveAxis = EGizmoAxis::Y;
        InitialIntersectionT = AxisTY;
    }
    if (DistZ < MinDist && AxisTZ > 0.0f)
    {
        MinDist = DistZ;
        ActiveAxis = EGizmoAxis::Z;
        InitialIntersectionT = AxisTZ;
    }

    if (ActiveAxis != EGizmoAxis::None)
    {
        bIsDragging = true;
        return true; // 기즈모 클릭 성공
    }

    return false; // 기즈모를 클릭하지 않음
}

void APivotTransformGizmo::OnMouseMove(const FVector<float> &RayOrigin, const FVector<float> &RayDir)
{
    if (!bIsDragging || TargetObject == nullptr || ActiveAxis == EGizmoAxis::None)
        return;

    FVector<float> GizmoOrigin = InitialObjectTransform.Location;
    FVector<float> CurrentAxisDir;

    switch (ActiveAxis)
    {
    case EGizmoAxis::X:
        CurrentAxisDir = FVector<float>(1.0f, 0.0f, 0.0f);
        break;
    case EGizmoAxis::Y:
        CurrentAxisDir = FVector<float>(0.0f, 1.0f, 0.0f);
        break;
    case EGizmoAxis::Z:
        CurrentAxisDir = FVector<float>(0.0f, 0.0f, 1.0f);
        break;
    default:
        return;
    }

    float OutAxisT, OutRayT;
    CalculateDistanceToAxis(RayOrigin, RayDir, GizmoOrigin, CurrentAxisDir, OutAxisT, OutRayT);

    // 축 상에서 마우스가 움직인 거리를 계산
    float       DeltaT = OutAxisT - InitialIntersectionT;
    FTransform  NewTransform = InitialObjectTransform;
    const float MinScale = 0.01f;
    const float TranslationSensitivity = 1.0f;
    const float RotationSensitivity = 20.0f;
    const float ScaleSensitivity = 0.05f;

    // 객체의 Transform 갱신 (이동 로직)

    switch (GizmoType)
    {
    case EGizmoHandleType::Translate:
        NewTransform.Location = InitialObjectTransform.Location + (CurrentAxisDir * DeltaT);
        break;
    case EGizmoHandleType::Rotate:
        FVector<float> RotationDelta = CurrentAxisDir * (DeltaT * RotationSensitivity);
        NewTransform.Rotation = InitialObjectTransform.Rotation + RotationDelta;
        break;
    case EGizmoHandleType::Scale:
        FVector<float> ScaleDelta = CurrentAxisDir * (DeltaT * ScaleSensitivity);
        NewTransform.Scale = InitialObjectTransform.Scale + ScaleDelta;

        // 스케일이 0이 되거나 뒤집히는(음수) 것을 방지하기 위한 최소값 보정
        if (NewTransform.Scale.X < MinScale)
            NewTransform.Scale.X = MinScale;
        if (NewTransform.Scale.Y < MinScale)
            NewTransform.Scale.Y = MinScale;
        if (NewTransform.Scale.Z < MinScale)
            NewTransform.Scale.Z = MinScale;
        break;
    default:
        break;
    }

    TargetObject->SetTransform(NewTransform);
}

void APivotTransformGizmo::OnMouseUp()
{
    bIsDragging = false;
    ActiveAxis = EGizmoAxis::None;
}

// Ray는 카메라에서 마우스 커서 방향으로 발사되는 벡터이며, Axis는 현재 Gizmo에서 결정된 축 방향 벡터이다.
// OutAxisT 변수는 Axis 방향으로 마우스가 이동한 거리를 나타내고, OutRayT 변수는 Ray 방향으로 마우스가 이동한 거리를 나타낸다.
// 이 함수는 Ray와 Axis(x, y, z축)가 가장 가까워지는 거리를 계산하여, 사용자가 현재 마우스로 클릭한 축을 찾는다.
float APivotTransformGizmo::CalculateDistanceToAxis(const FVector<float> &RayOrigin, const FVector<float> &RayDir, const FVector<float> &AxisOrigin,
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