#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/PivotTransformGizmo.h"

APivotTransformGizmo::APivotTransformGizmo()
{
    const float HALF_PI = 1.570796f;

    UArrowComponent *TranslateX = new UArrowComponent();
    TranslateX->SetRotation({0.0, 0.0f, 0.0f});
    TranslateX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    TranslateX->SetAlwaysVisible(true);
    TranslateX->SetCullMode(ECullMode::None);
    TranslateGizmoComponents.push_back(TranslateX);

    UArrowComponent *TranslateY = new UArrowComponent();
    TranslateY->SetRotation({HALF_PI, 0.0f, 0.0f});
    TranslateY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    TranslateY->SetAlwaysVisible(true);
    TranslateY->SetCullMode(ECullMode::None);
    TranslateGizmoComponents.push_back(TranslateY);

    UArrowComponent *TranslateZ = new UArrowComponent();
    TranslateZ->SetRotation({0.0f, -HALF_PI, 0.0f});
    TranslateZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    TranslateZ->SetAlwaysVisible(true);
    TranslateZ->SetCullMode(ECullMode::None);
    TranslateGizmoComponents.push_back(TranslateZ);

    URingComponent *RotateX = new URingComponent();
    RotateX->SetRotation({0.0f, 0.0f, 0.0f});
    RotateX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    RotateX->SetAlwaysVisible(true);
    RotateX->SetCullMode(ECullMode::None);
    RotateGizmoComponents.push_back(RotateX);

    URingComponent *RotateY = new URingComponent();
    RotateY->SetRotation({HALF_PI, 0.0f, 0.0f});
    RotateY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    RotateY->SetAlwaysVisible(true);
    RotateY->SetCullMode(ECullMode::None);
    RotateGizmoComponents.push_back(RotateY);

    URingComponent *RotateZ = new URingComponent();
    RotateZ->SetRotation({0.0f, -HALF_PI, 0.0f});
    RotateZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    RotateZ->SetAlwaysVisible(true);
    RotateZ->SetCullMode(ECullMode::None);
    RotateGizmoComponents.push_back(RotateZ);

    UCubeArrowComponent *ScaleX = new UCubeArrowComponent();
    ScaleX->SetRotation({0.0f, 0.0f, 0.0f});
    ScaleX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    ScaleX->SetAlwaysVisible(true);
    ScaleX->SetCullMode(ECullMode::None);
    ScaleGizmoComponents.push_back(ScaleX);

    UCubeArrowComponent *ScaleY = new UCubeArrowComponent();
    ScaleY->SetRotation({HALF_PI, 0.0f, 0.0f});
    ScaleY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    ScaleY->SetAlwaysVisible(true);
    ScaleY->SetCullMode(ECullMode::None);
    ScaleGizmoComponents.push_back(ScaleY);

    UCubeArrowComponent *ScaleZ = new UCubeArrowComponent();
    ScaleZ->SetRotation({0.0f, -HALF_PI, 0.0f});
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

void APivotTransformGizmo::Render(URenderer &renderer, const FMatrix<float>& ViewMatrix)
{
    if (TargetObject == nullptr)
        return;

    FTransform TargetTransform = TargetObject->GetTransform();
    FTransform UnscaledTransform;

    UnscaledTransform.Location = TargetTransform.Location;

    if (bIsDragging)
        UnscaledTransform.Rotation = InitialObjectTransform.Rotation;
    else
        UnscaledTransform.Rotation = TargetTransform.Rotation;

    // 타겟의 3D 월드 위치를 Vector4로 구성한 뒤, ViewMatrix를 곱해 카메라 기준 로컬 좌표로 변환한다.
    // 이 떄의 Z값이 곧 물체부터 카메라까지의 정확한 깊이이다.
    FVector4<float> TargetWorldPos(TargetTransform.Location.X, TargetTransform.Location.Y, TargetTransform.Location.Z, 1.0f);
    FVector4<float> TargetViewPos = TargetWorldPos * ViewMatrix;

    float Distance = std::abs(TargetViewPos.Z);

    if (Distance < 0.01f) Distance = 0.01f; // 거리가 너무 가까울 때 사라짐 방지

    const float GizmoScreenSize = 0.2f; 
    float ScaleFactor = Distance * GizmoScreenSize;
    UnscaledTransform.Scale = FVector<float>(ScaleFactor, ScaleFactor, ScaleFactor);

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

    // 1. 현재 모드(Translate, Rotate, Scale)에 맞는 기즈모 컴포넌트 배열 선택
    std::vector<UPrimitiveComponent*>* ActiveComponents = nullptr;
    switch (GizmoType)
    {
    case EGizmoHandleType::Translate: ActiveComponents = &TranslateGizmoComponents; break;
    case EGizmoHandleType::Rotate:    ActiveComponents = &RotateGizmoComponents; break;
    case EGizmoHandleType::Scale:     ActiveComponents = &ScaleGizmoComponents; break;
    }

    if (ActiveComponents == nullptr) 
        return false;

    // 2. 실제 기즈모 메쉬(삼각형) 기반 Ray 충돌 검사
    float MinDistance = FLT_MAX;
    int HitIndex = -1;

    // 배열 인덱스 0: X축, 1: Y축, 2: Z축
    for (int i = 0; i < ActiveComponents->size(); ++i)
    {
        UPrimitiveComponent* Comp = (*ActiveComponents)[i];
        if (Comp == nullptr) continue;

        FHitResult Hit = Comp->IntersectRay(RayOrigin, RayDir);
        
        if (Hit.bHit && Hit.Distance < MinDistance)
        {
            MinDistance = Hit.Distance;
            HitIndex = i;
        }
    }

    // 아무 기즈모 메쉬도 클릭하지 않고 허공을 클릭함
    if (HitIndex == -1)
        return false; 

    // 3. 충돌한 인덱스를 바탕으로 조작할 축(ActiveAxis) 설정
    if (HitIndex == 0) ActiveAxis = EGizmoAxis::Z;
    else if (HitIndex == 1) ActiveAxis = EGizmoAxis::Y;
    else if (HitIndex == 2) ActiveAxis = EGizmoAxis::X;

    // 4. 마우스 이동 거리 계산을 위한 가상 드래그 평면 초기화 로직
    FMatrix<float> RotationMatrix = FRotationMatrix<float>(InitialObjectTransform.Rotation);
    FVector4<float> Dir;
    
    if (ActiveAxis == EGizmoAxis::X) Dir = FVector4<float>(1.0f, 0.0f, 0.0f, 0.0f) * RotationMatrix;
    else if (ActiveAxis == EGizmoAxis::Y) Dir = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
    else if (ActiveAxis == EGizmoAxis::Z) Dir = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;

    FVector<float> AxisDir(Dir.X, Dir.Y, Dir.Z);
    AxisDir.Normalize();

    FVector<float> CameraDir = RayDir; // 클릭 시점의 카메라 시선
    FVector<float> RightVec = FVector<float>::CrossProduct(AxisDir, CameraDir);
    DragPlaneNormal = FVector<float>(-RayDir.X, -RayDir.Y, -RayDir.Z);
    DragPlaneNormal.Normalize();
    
    float Denom = FVector<float>::DotProduct(RayDir, DragPlaneNormal);
    if (std::abs(Denom) > 0.0001f)
    {
        float t = FVector<float>::DotProduct(InitialObjectTransform.Location - RayOrigin, DragPlaneNormal) / Denom;
        FVector<float> HitPoint = RayOrigin + (RayDir * t);
        
        // 부딪힌 3D 좌표를 기즈모 축 위로 투영하여 1D 스칼라 값으로 변환
        InitialIntersectionT = FVector<float>::DotProduct(HitPoint - InitialObjectTransform.Location, AxisDir);
    }
    else
    {
        InitialIntersectionT = 0.0f;
    }

    bIsDragging = true;
    return true;
}

void APivotTransformGizmo::OnMouseMove(const FVector<float> &RayOrigin, const FVector<float> &RayDir)
{
    if (!bIsDragging || TargetObject == nullptr || ActiveAxis == EGizmoAxis::None)
        return;

    FVector<float>  GizmoOrigin = InitialObjectTransform.Location;
    FVector<float>  AxisDir;
    FVector4<float> Dir;

    FMatrix<float> RotationMatrix = FRotationMatrix<float>(InitialObjectTransform.Rotation);

    switch (ActiveAxis)
    {
    case EGizmoAxis::X:
        Dir = FVector4<float>(1.0f, 0.0f, 0.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
        break;
    case EGizmoAxis::Y:
        Dir = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
        break;
    case EGizmoAxis::Z:
        Dir = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
        break;
    default:
        return;
    }
    
    AxisDir.Normalize();

    float Denom = FVector<float>::DotProduct(RayDir, DragPlaneNormal);
    
    // 폭주 방지: 마우스 Ray가 평면과 완벽히 수평이 되는 경우는 이동 무시
    if (std::abs(Denom) <= 0.0001f) 
        return; 

    // 1. 가상 평면과의 정확한 3D 충돌점 계산
    float t = FVector<float>::DotProduct(GizmoOrigin - RayOrigin, DragPlaneNormal) / Denom;
    FVector<float> HitPoint = RayOrigin + (RayDir * t);

    // 2. 충돌점을 조작 중인 축 방향으로 투영하여 현재 이동량(CurrentAxisT) 도출
    float AxisT = FVector<float>::DotProduct(HitPoint - GizmoOrigin, AxisDir);

    // 3. 최종적으로 마우스가 축 위에서 얼마나 움직였는지 순수 거리(DeltaT) 산출
    float DeltaT = AxisT - InitialIntersectionT;

    FTransform NewTransform = InitialObjectTransform;
    const float MinScale = 0.01f;
    const float TranslationSensitivity = 1.0f;
    const float RotationSensitivity = 5.0f;
    const float ScaleSensitivity = 0.1f;

    // 객체의 Transform 갱신
    switch (GizmoType)
    {
    case EGizmoHandleType::Translate:
        NewTransform.Location = InitialObjectTransform.Location + (AxisDir * (DeltaT * TranslationSensitivity));
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

        if (NewTransform.Scale.X < MinScale) NewTransform.Scale.X = MinScale;
        if (NewTransform.Scale.Y < MinScale) NewTransform.Scale.Y = MinScale;
        if (NewTransform.Scale.Z < MinScale) NewTransform.Scale.Z = MinScale;
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