#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/PivotTransformGizmo.h"

APivotTransformGizmo::APivotTransformGizmo() 
{
    UArrowComponent* TranslateX = new UArrowComponent();
    TranslateX->SetRotation({1.0f, 0.0f, 0.0f});
    TranslateX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    TranslateGizmoComponents.push_back(TranslateX);

    UArrowComponent* TranslateY = new UArrowComponent();
    TranslateY->SetRotation({0.0f, 1.0f, 0.0f});
    TranslateY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    TranslateGizmoComponents.push_back(TranslateY);

    UArrowComponent* TranslateZ = new UArrowComponent();
    TranslateZ->SetRotation({0.0f, 0.0f, 1.0f});
    TranslateZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    TranslateGizmoComponents.push_back(TranslateZ);

    URingComponent* RotateX = new URingComponent();
    RotateX->SetRotation({1.0f, 0.0f, 0.0f});
    RotateX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    RotateGizmoComponents.push_back(RotateX);

    URingComponent* RotateY = new URingComponent();
    RotateY->SetRotation({0.0f, 1.0f, 0.0f});
    RotateY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    RotateGizmoComponents.push_back(RotateY);

    URingComponent* RotateZ = new URingComponent();
    RotateZ->SetRotation({0.0f, 0.0f, 1.0f});
    RotateZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    RotateGizmoComponents.push_back(RotateZ);

    UCubeArrowComponent* ScaleX = new UCubeArrowComponent();
    ScaleX->SetRotation({1.0f, 0.0f, 0.0f});
    ScaleX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    ScaleGizmoComponents.push_back(ScaleX);

    UCubeArrowComponent* ScaleY = new UCubeArrowComponent();
    ScaleY->SetRotation({0.0f, 1.0f, 0.0f});
    ScaleY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    ScaleGizmoComponents.push_back(ScaleY);

    UCubeArrowComponent* ScaleZ = new UCubeArrowComponent();
    ScaleZ->SetRotation({0.0f, 0.0f, 1.0f});
    ScaleZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    ScaleGizmoComponents.push_back(ScaleZ);

    GizmoType = EGizmoHandleType::Translate;
}

APivotTransformGizmo::~APivotTransformGizmo()
{
    for (auto *Component : TranslateGizmoComponents)
    {
        if (Component != nullptr)
        {
            delete Component;
        }
    }
    TranslateGizmoComponents.clear();

    for (auto *Component : RotateGizmoComponents)
    {
        if (Component != nullptr)
        {
            delete Component;
        }
    }
    RotateGizmoComponents.clear();

    // 3. Scale 기즈모 메모리 해제
    for (auto *Component : ScaleGizmoComponents)
    {
        if (Component != nullptr)
        {
            delete Component;
        }
    }
    ScaleGizmoComponents.clear();
}

void APivotTransformGizmo::Render(URenderer &renderer)
{
    // if (TargetObject == nullptr)
    //    return;

    switch (GizmoType)
    {
    case EGizmoHandleType::Translate:
        for (auto GizmoComponent : TranslateGizmoComponents)
        {
            if (GizmoComponent != nullptr)
                GizmoComponent->Render(renderer);
        }
        break;
    case EGizmoHandleType::Rotate:

        for (auto *GizmoComponent : RotateGizmoComponents)
        {
            if (GizmoComponent != nullptr)
                GizmoComponent->Render(renderer);
        }
        break;
    case EGizmoHandleType::Scale:

        for (auto *GizmoComponent : ScaleGizmoComponents)
        {
            if (GizmoComponent != nullptr)
                GizmoComponent->Render(renderer);
        }
        break;
    }
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

void APivotTransformGizmo::OnNewObjectsSelected()
{
}

void APivotTransformGizmo::ToggleMode()
{
    uint32 CurrentModeIndex = static_cast<uint32>(GizmoType);
    uint32 NextModeIndex = (CurrentModeIndex + 1) % 3;
    GizmoType = static_cast<EGizmoHandleType>(CurrentModeIndex);
}