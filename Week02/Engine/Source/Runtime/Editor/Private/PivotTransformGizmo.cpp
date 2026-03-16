#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/PivotTransformGizmo.h"

APivotTransformGizmo::APivotTransformGizmo()
{
    const float HALF_PI = 1.570796f;

    UArrowComponent *TranslateX = new UArrowComponent();
    TranslateX->SetRotation({HALF_PI, 0.0f, 0.0f});
    TranslateX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    TranslateX->SetAlwaysVisible(true);
    TranslateX->SetCullMode(ECullMode::None);
    TranslateGizmoComponents.push_back(TranslateX);

    UArrowComponent *TranslateY = new UArrowComponent();
    TranslateY->SetRotation({0.0f, -HALF_PI, 0.0f});
    TranslateY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    TranslateY->SetAlwaysVisible(true);
    TranslateY->SetCullMode(ECullMode::None);
    TranslateGizmoComponents.push_back(TranslateY);

    UArrowComponent *TranslateZ = new UArrowComponent();
    TranslateZ->SetRotation({0.0f, 0.0f, HALF_PI});
    TranslateZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    TranslateZ->SetAlwaysVisible(true);
    TranslateZ->SetCullMode(ECullMode::None);
    TranslateGizmoComponents.push_back(TranslateZ);

    URingComponent *RotateX = new URingComponent();
    RotateX->SetRotation({HALF_PI, 0.0f, 0.0f});
    RotateX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    RotateX->SetAlwaysVisible(true);
    RotateX->SetCullMode(ECullMode::None);
    RotateGizmoComponents.push_back(RotateX);

    URingComponent *RotateY = new URingComponent();
    RotateY->SetRotation({0.0f, -HALF_PI, 0.0f});
    RotateY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    RotateY->SetAlwaysVisible(true);
    RotateY->SetCullMode(ECullMode::None);
    RotateGizmoComponents.push_back(RotateY);

    URingComponent *RotateZ = new URingComponent();
    RotateZ->SetRotation({0.0f, 0.0f, HALF_PI});
    RotateZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    RotateZ->SetAlwaysVisible(true);
    RotateZ->SetCullMode(ECullMode::None);
    RotateGizmoComponents.push_back(RotateZ);

    UCubeArrowComponent *ScaleX = new UCubeArrowComponent();
    ScaleX->SetRotation({HALF_PI, 0.0f, 0.0f});
    ScaleX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    ScaleX->SetAlwaysVisible(true);
    ScaleX->SetCullMode(ECullMode::None);
    ScaleGizmoComponents.push_back(ScaleX);

    UCubeArrowComponent *ScaleY = new UCubeArrowComponent();
    ScaleY->SetRotation({0.0f, -HALF_PI, 0.0f});
    ScaleY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    ScaleY->SetAlwaysVisible(true);
    ScaleY->SetCullMode(ECullMode::None);
    ScaleGizmoComponents.push_back(ScaleY);

    UCubeArrowComponent *ScaleZ = new UCubeArrowComponent();
    ScaleZ->SetRotation({0.0f, 0.0f, 1.0f});
    ScaleZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    ScaleZ->SetAlwaysVisible(true);
    ScaleZ->SetCullMode(ECullMode::None);
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
    if (TargetObject == nullptr)
        return;

    FTransform TargetTransform = TargetObject->GetTransform();

    FTransform UnscaledTransform;
    UnscaledTransform.Location = TargetTransform.Location;
    UnscaledTransform.Rotation = TargetTransform.Rotation;
    UnscaledTransform.Scale = FVector<float>(1.0f, 1.0f, 1.0f); // 스케일 무시

    FMatrix<float> TargetMatrix = UnscaledTransform.ToMatrix();

    switch (GizmoType)
    {
    case EGizmoHandleType::Translate:
        for (auto GizmoComponent : TranslateGizmoComponents)
        {
            if (GizmoComponent != nullptr)
            {
                // 타겟의 변환 행렬을 부모 행렬로 전달 (자신의 고유 Transform은 유지됨)
                GizmoComponent->SetParentMatrix(TargetMatrix);
                GizmoComponent->Render(renderer);
            }
        }
        break;
    case EGizmoHandleType::Rotate:

        for (auto *GizmoComponent : RotateGizmoComponents)
        {
            if (GizmoComponent != nullptr)
            {
                // 타겟의 변환 행렬을 부모 행렬로 전달 (자신의 고유 Transform은 유지됨)
                GizmoComponent->SetParentMatrix(TargetMatrix);
                GizmoComponent->Render(renderer);
            }
        }
        break;
    case EGizmoHandleType::Scale:

        for (auto *GizmoComponent : ScaleGizmoComponents)
        {
            if (GizmoComponent != nullptr)
            {
                // 타겟의 변환 행렬을 부모 행렬로 전달 (자신의 고유 Transform은 유지됨)
                GizmoComponent->SetParentMatrix(TargetMatrix);
                GizmoComponent->Render(renderer);
            }
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

    // 오브젝트의 현재 회전값을 바탕으로 회전 행렬 생성
    FMatrix<float> RotationMatrix = FRotationMatrix<float>(InitialObjectTransform.Rotation);

    // 로컬 축(Local Axis)에 회전 행렬을 곱해 월드 공간에서의 방향 벡터 도출
    FVector4<float> WorldX = FVector4<float>(1.0f, 0.0f, 0.0f, 0.0f) * RotationMatrix;
    FVector4<float> WorldY = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
    FVector4<float> WorldZ = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;

    FVector<float> AxisDirX(WorldX.X, WorldX.Y, WorldX.Z);
    FVector<float> AxisDirY(WorldY.X, WorldY.Y, WorldY.Z);
    FVector<float> AxisDirZ(WorldZ.X, WorldZ.Y, WorldZ.Z);

    // 정규화
    AxisDirX.Normalize();
    AxisDirY.Normalize();
    AxisDirZ.Normalize();

    // 기즈모를 선택하기 위한 허용 오차 (클릭 범위)
    const float SelectThreshold = 0.2f;

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

    FVector<float>  GizmoOrigin = InitialObjectTransform.Location;
    FVector<float>  CurrentAxisDir;
    FVector4<float> Dir;

    FMatrix<float> RotationMatrix = FRotationMatrix<float>(InitialObjectTransform.Rotation);

    switch (ActiveAxis)
    {
    case EGizmoAxis::X:
        Dir = FVector4<float>(1.0f, 0.0f, 0.0f, 0.0f) * RotationMatrix;
        CurrentAxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
        break;
    case EGizmoAxis::Y:
        Dir = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
        CurrentAxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
        break;
    case EGizmoAxis::Z:
        Dir = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;
        CurrentAxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
        break;
    default:
        return;
    }
    
    CurrentAxisDir.Normalize();

    float OutAxisT, OutRayT;
    CalculateDistanceToAxis(RayOrigin, RayDir, GizmoOrigin, CurrentAxisDir, OutAxisT, OutRayT);

    // 축 상에서 마우스가 움직인 거리를 계산
    float       DeltaT = OutAxisT - InitialIntersectionT;
    FTransform  NewTransform = InitialObjectTransform;
    const float MinScale = 0.01f;
    const float TranslationSensitivity = 1.0f;
    const float RotationSensitivity = 5.0f;
    const float ScaleSensitivity = 0.1f;

    // 객체의 Transform 갱신 (이동 로직)
    switch (GizmoType)
    {
    case EGizmoHandleType::Translate:
        NewTransform.Location = InitialObjectTransform.Location + (CurrentAxisDir * (DeltaT * TranslationSensitivity));
        break;
    case EGizmoHandleType::Rotate:
        if (ActiveAxis == EGizmoAxis::X)
            NewTransform.Rotation.X += DeltaT * RotationSensitivity;
        else if (ActiveAxis == EGizmoAxis::Y)
            NewTransform.Rotation.Y += DeltaT * RotationSensitivity;
        else if (ActiveAxis == EGizmoAxis::Z)
            NewTransform.Rotation.Z += DeltaT * RotationSensitivity;
        break;
    case EGizmoHandleType::Scale:
        if (ActiveAxis == EGizmoAxis::X)
            NewTransform.Scale.X += DeltaT * ScaleSensitivity;
        else if (ActiveAxis == EGizmoAxis::Y)
            NewTransform.Scale.Y += DeltaT * ScaleSensitivity;
        else if (ActiveAxis == EGizmoAxis::Z)
            NewTransform.Scale.Z += DeltaT * ScaleSensitivity;

        if (NewTransform.Scale.X < MinScale)
            NewTransform.Scale.X = MinScale;
        if (NewTransform.Scale.Y < MinScale)
            NewTransform.Scale.Y = MinScale;
        if (NewTransform.Scale.Z < MinScale)
            NewTransform.Scale.Z = MinScale;
        break;
    }

    TargetObject->SetTransform(NewTransform);
}

void APivotTransformGizmo::OnMouseUp()
{
    bIsDragging = false;
    ActiveAxis = EGizmoAxis::None;
}

void APivotTransformGizmo::ToggleMode()
{
    if (TargetObject == nullptr)
        return;

    uint32 CurrentModeIndex = static_cast<uint32>(GizmoType);
    uint32 NextModeIndex = (CurrentModeIndex + 1) % 3;
    GizmoType = static_cast<EGizmoHandleType>(NextModeIndex);
}