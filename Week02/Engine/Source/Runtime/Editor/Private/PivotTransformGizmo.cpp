#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/PivotTransformGizmo.h"

APivotTransformGizmo::APivotTransformGizmo(const FString &InString) : ABaseTransformGizmo(InString)
{
    USceneComponent *Root = new USceneComponent("PivotTransformSceneComponent");
    this->SetRootComponent(Root);
    Root->RegisterComponent();

    const float HALF_PI = 1.570796f;

    UArrowComponent *TranslateX = new UArrowComponent("XArrowComponent");
    TranslateX->SetOuter(this);
    TranslateX->RegisterComponent();
    TranslateX->SetRotation({0.0, -HALF_PI, 0.0f});
    TranslateX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    TranslateX->SetCullMode(ECullMode::None);
    TranslateX->SetAlwaysVisible(true);
    TranslateGizmoComponents.push_back(TranslateX);

    UArrowComponent *TranslateY = new UArrowComponent("YArrowComponent");
    TranslateY->SetOuter(this);
    TranslateY->RegisterComponent();
    TranslateY->SetRotation({HALF_PI, 0.0f, 0.0f});
    TranslateY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    TranslateY->SetCullMode(ECullMode::None);
    TranslateY->SetAlwaysVisible(true);
    TranslateGizmoComponents.push_back(TranslateY);

    UArrowComponent *TranslateZ = new UArrowComponent("ZArrowComponent");
    TranslateZ->SetOuter(this);
    TranslateZ->RegisterComponent();
    TranslateZ->SetRotation({0.0f, 0.0f, 0.0f});
    TranslateZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    TranslateZ->SetCullMode(ECullMode::None);
    TranslateZ->SetAlwaysVisible(true);
    TranslateGizmoComponents.push_back(TranslateZ);

    URingComponent *RotateX = new URingComponent("XRingComponent");
    RotateX->SetOuter(this);
    RotateX->RegisterComponent();
    RotateX->SetRotation({0.0f, -HALF_PI, 0.0f});
    RotateX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    RotateX->SetCullMode(ECullMode::None);
    RotateX->SetAlwaysVisible(true);
    RotateGizmoComponents.push_back(RotateX);

    URingComponent *RotateY = new URingComponent("YRingComponent");
    RotateY->SetOuter(this);
    RotateY->RegisterComponent();
    RotateY->SetRotation({HALF_PI, 0.0f, 0.0f});
    RotateY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    RotateY->SetCullMode(ECullMode::None);
    RotateY->SetAlwaysVisible(true);
    RotateGizmoComponents.push_back(RotateY);

    URingComponent *RotateZ = new URingComponent("ZRingComponent");
    RotateZ->SetOuter(this);
    RotateZ->RegisterComponent();
    RotateZ->SetRotation({0.0f, 0.0f, 0.0f});
    RotateZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    RotateZ->SetCullMode(ECullMode::None);
    RotateZ->SetAlwaysVisible(true);
    RotateGizmoComponents.push_back(RotateZ);

    UCubeArrowComponent *ScaleX = new UCubeArrowComponent("XCubeArrowComponent");
    ScaleX->SetOuter(this);
    ScaleX->RegisterComponent();
    ScaleX->SetRotation({0.0f, -HALF_PI, 0.0f});
    ScaleX->SetColor({1.0f, 0.0f, 0.0f, 1.0f});
    ScaleX->SetCullMode(ECullMode::None);
    ScaleX->SetAlwaysVisible(true);
    ScaleGizmoComponents.push_back(ScaleX);

    UCubeArrowComponent *ScaleY = new UCubeArrowComponent("YCubeArrowComponent");
    ScaleY->SetOuter(this);
    ScaleY->RegisterComponent();
    ScaleY->SetRotation({HALF_PI, 0.0f, 0.0f});
    ScaleY->SetColor({0.0f, 1.0f, 0.0f, 1.0f});
    ScaleY->SetCullMode(ECullMode::None);
    ScaleY->SetAlwaysVisible(true);
    ScaleGizmoComponents.push_back(ScaleY);

    UCubeArrowComponent *ScaleZ = new UCubeArrowComponent("ZCubeArrowComponent");
    ScaleZ->SetOuter(this);
    ScaleZ->RegisterComponent();
    ScaleZ->SetRotation({0.0f, 0.0f, 0.0f});
    ScaleZ->SetColor({0.0f, 0.0f, 1.0f, 1.0f});
    ScaleZ->SetCullMode(ECullMode::None);
    ScaleZ->SetAlwaysVisible(true);
    ScaleGizmoComponents.push_back(ScaleZ);

    GizmoType = EGizmoHandleType::Translate;
}

APivotTransformGizmo::~APivotTransformGizmo()
{
}

void APivotTransformGizmo::Render(URenderer &renderer, const FMatrix<float> &ViewMatrix)
{
    if (TargetObject == nullptr)
        return;

    FTransform TargetTransform = TargetObject->GetTransform();
    FTransform UnscaledTransform;

    UnscaledTransform.Location = TargetTransform.Location;
    UnscaledTransform.Rotation = TargetTransform.Rotation;

    // 타겟의 3D 월드 위치를 Vector4로 구성한 뒤, ViewMatrix를 곱해 카메라 기준 로컬 좌표로 변환한다.
    // 이 떄의 Z값이 곧 물체부터 카메라까지의 정확한 깊이이다.
    FVector4<float> TargetWorldPos(TargetTransform.Location.X, TargetTransform.Location.Y, TargetTransform.Location.Z, 1.0f);
    FVector4<float> TargetViewPos = TargetWorldPos * ViewMatrix;

    float Distance = std::abs(TargetViewPos.Z);

    if (Distance < 0.01f)
        Distance = 0.01f; // 거리가 너무 가까울 때 사라짐 방지

    float ScaleFactor = 1.25f;

    if (!UImGuiManager::Get().bIsOrthogonal)
    {
        ScaleFactor = 0.25f;
        // 타겟의 3D 월드 위치를 Vector4로 구성한 뒤, ViewMatrix를 곱해 카메라 기준 로컬 좌표로 변환한다.
        // 이 떄의 Z값이 곧 물체부터 카메라까지의 정확한 깊이이다.
        FVector4<float> TargetWorldPos(TargetTransform.Location.X, TargetTransform.Location.Y, TargetTransform.Location.Z, 1.0f);
        FVector4<float> TargetViewPos = TargetWorldPos * ViewMatrix;

        float Distance = std::abs(TargetViewPos.Z);

        if (Distance < 0.01f)
            Distance = 0.01f; // 거리가 너무 가까울 때 사라짐 방지

        ScaleFactor *= Distance;
    }

    UnscaledTransform.Scale = FVector<float>(ScaleFactor, ScaleFactor, ScaleFactor);

    this->SetTransform(UnscaledTransform);

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

    // 1. 현재 모드(Translate, Rotate, Scale)에 맞는 기즈모 컴포넌트 배열 선택
    std::vector<UPrimitiveComponent *> *ActiveComponents = nullptr;
    switch (GizmoType)
    {
    case EGizmoHandleType::Translate:
        ActiveComponents = &TranslateGizmoComponents;
        break;
    case EGizmoHandleType::Rotate:
        ActiveComponents = &RotateGizmoComponents;
        break;
    case EGizmoHandleType::Scale:
        ActiveComponents = &ScaleGizmoComponents;
        break;
    }

    if (ActiveComponents == nullptr)
        return false;

    // 2. 실제 기즈모 메쉬(삼각형) 기반 Ray 충돌 검사
    float MinDistance = FLT_MAX;
    int   HitIndex = -1;

    // 배열 인덱스 0: X축, 1: Y축, 2: Z축
    for (int i = 0; i < ActiveComponents->size(); ++i)
    {
        UPrimitiveComponent *Comp = (*ActiveComponents)[i];
        if (Comp == nullptr)
            continue;

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
    if (HitIndex == 0)
        ActiveAxis = EGizmoAxis::X;
    else if (HitIndex == 1)
        ActiveAxis = EGizmoAxis::Y;
    else if (HitIndex == 2)
        ActiveAxis = EGizmoAxis::Z;

    // 4. 마우스 이동 거리 계산을 위한 가상 드래그 평면 초기화 로직
    FMatrix<float>  RotationMatrix = FRotationMatrix<float>(InitialObjectTransform.Rotation);
    FVector4<float> Dir;

    if (ActiveAxis == EGizmoAxis::X)
        Dir = FVector4<float>(1.0f, 0.0f, 0.0f, 0.0f) * RotationMatrix;
    else if (ActiveAxis == EGizmoAxis::Y)
        Dir = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
    else if (ActiveAxis == EGizmoAxis::Z)
        Dir = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;

    FVector<float> AxisDir(Dir.X, Dir.Y, Dir.Z);
    AxisDir.Normalize();

    FVector<float> CameraDir = RayDir; // 클릭 시점의 카메라 시선
    FVector<float> RightVec = FVector<float>::CrossProduct(AxisDir, CameraDir);
    GizmoPlaneNormal = FVector<float>(-RayDir.X, -RayDir.Y, -RayDir.Z);
    GizmoPlaneNormal.Normalize();

    if (GizmoType == EGizmoHandleType::Rotate)
    {
        // [수정 1] 회전 모드: 조작할 축(AxisDir) 자체가 평면의 법선(Normal)이 됩니다.
        GizmoPlaneNormal = AxisDir;
        float Denom = FVector<float>::DotProduct(RayDir, AxisDir);
        if (std::abs(Denom) > 0.001f)
        {
            float          t = FVector<float>::DotProduct(InitialObjectTransform.Location - RayOrigin, AxisDir) / Denom;
            FVector<float> HitPoint = RayOrigin + (RayDir * t);
            InitialDragVector = HitPoint - InitialObjectTransform.Location;
            InitialDragVector.Normalize();
        }
    }
    else
    {
        // 이동/스케일 모드: 기존처럼 카메라를 바라보는 평면을 사용합니다.
        GizmoPlaneNormal = FVector<float>(-RayDir.X, -RayDir.Y, -RayDir.Z);
        GizmoPlaneNormal.Normalize();

        float Denom = FVector<float>::DotProduct(RayDir, GizmoPlaneNormal);
        if (std::abs(Denom) > 0.0001f)
        {
            float          t = FVector<float>::DotProduct(InitialObjectTransform.Location - RayOrigin, GizmoPlaneNormal) / Denom;
            FVector<float> HitPoint = RayOrigin + (RayDir * t);
            InitialRayDistance = FVector<float>::DotProduct(HitPoint - InitialObjectTransform.Location, AxisDir);
        }
        else
        {
            InitialRayDistance = 0.0f;
        }
    }

    bIsDragging = true;
    return true;
}

void APivotTransformGizmo::OnMouseMove(const FVector<float> &RayOrigin, const FVector<float> &RayDir)
{
    if (TargetObject == nullptr || ActiveAxis == EGizmoAxis::None)
        return;

    FVector<float> GizmoOrigin = InitialObjectTransform.Location;
    FVector<float> AxisDir;

    // 1. 현재 조작 중인 축(로컬 축 기준) 도출
    FMatrix<float> RotationMatrix = FRotationMatrix<float>(InitialObjectTransform.Rotation);
    switch (ActiveAxis)
    {
    case EGizmoAxis::X:
    {
        FVector4<float> Dir = FVector4<float>(1.0f, 0.0f, 0.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
    }
    break;
    case EGizmoAxis::Y:
    {
        FVector4<float> Dir = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
    }
    break;
    case EGizmoAxis::Z:
    {
        FVector4<float> Dir = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
    }
    break;
    default:
        return;
    }

    AxisDir.Normalize();
    FTransform NewTransform = InitialObjectTransform;

    // ==========================================
    // 회전 기즈모 (Rotation)
    // ==========================================
    if (GizmoType == EGizmoHandleType::Rotate)
    {
        // 조작 축 자체가 마우스 투영 평면의 법선(Normal)
        FVector<float> PlaneNormal = AxisDir;
        float          Denom = FVector<float>::DotProduct(RayDir, PlaneNormal);

        // [안전장치 1] 카메라 시선이 링 평면과 완전히 수평이 되어 값이 무한대로 폭주하는 현상 차단
        if (std::abs(Denom) < 0.001f)
            return;

        float t = FVector<float>::DotProduct(GizmoOrigin - RayOrigin, PlaneNormal) / Denom;

        // [안전장치 2] 마우스 Ray의 충돌점이 카메라 뒤쪽에 형성되어 각도가 역전되는 현상 차단
        if (t < 0.0f)
            return;

        // 평면 상의 마우스 3D 충돌점 도출
        FVector<float> HitPoint = RayOrigin + (RayDir * t);

        // 기즈모 중심에서 현재 마우스 위치까지의 방향 벡터
        FVector<float> CurrentDragVector = HitPoint - GizmoOrigin;
        CurrentDragVector.Normalize();

        // [핵심 로직] acos의 도메인 에러(값이 튀는 현상)를 배제하고 정확한 부호와 각도를 얻기 위해 atan2 사용
        FVector<float> CrossProduct = FVector<float>::CrossProduct(InitialDragVector, CurrentDragVector);
        float          y = FVector<float>::DotProduct(CrossProduct, PlaneNormal);            // sin 성분 (외적 벡터를 축에 투영)
        float          x = FVector<float>::DotProduct(InitialDragVector, CurrentDragVector); // cos 성분 (두 벡터의 내적)

        // 1. 타겟 오브젝트의 현재 회전 상태를 행렬로 변환
        FMatrix<float> CurrentRotMat = FRotationMatrix<float>(InitialObjectTransform.Rotation);

        float DeltaAngle = -std::atan2(y, x);

        // 2. 조작한 축을 기준으로 '순수한 로컬 회전(Delta) 행렬' 생성
        FMatrix<float> DeltaRotMat;
        if (ActiveAxis == EGizmoAxis::X)
            DeltaRotMat = FRotationXMatrix<float>(DeltaAngle);
        else if (ActiveAxis == EGizmoAxis::Y)
            DeltaRotMat = FRotationYMatrix<float>(DeltaAngle);
        else if (ActiveAxis == EGizmoAxis::Z)
            DeltaRotMat = FRotationZMatrix<float>(DeltaAngle);

        // 3. 로컬 회전 누적 (행렬 곱셈)
        // 행벡터(Row-vector) 기반 수학에서는 Delta 행렬을 '앞'에 곱해야 로컬 축 기준 회전이 됩니다.
        FMatrix<float> NewRotMat = DeltaRotMat * CurrentRotMat;

        // 4. 새로운 회전 행렬에서 오일러 각 추출하여 Transform에 갱신
        NewTransform.Rotation = NewRotMat.ToEuler();
    }
    // ==========================================
    // 이동 / 스케일 기즈모 (Translation / Scale)
    // ==========================================
    else
    {
        float Denom = FVector<float>::DotProduct(RayDir, GizmoPlaneNormal);

        // [안전장치] 평행 및 뒤쪽 투영 방지
        if (std::abs(Denom) < 0.001f)
            return;

        float t = FVector<float>::DotProduct(GizmoOrigin - RayOrigin, GizmoPlaneNormal) / Denom;
        if (t < 0.0f)
            return;

        FVector<float> HitPoint = RayOrigin + (RayDir * t);
        float          AxisT = FVector<float>::DotProduct(HitPoint - GizmoOrigin, AxisDir);
        float          DeltaT = AxisT - InitialRayDistance;

        if (GizmoType == EGizmoHandleType::Translate)
        {
            NewTransform.Location = InitialObjectTransform.Location + (AxisDir * DeltaT);
        }
        else if (GizmoType == EGizmoHandleType::Scale)
        {
            const float ScaleSensitivity = 0.2f;
            const float MinScale = 0.01f;

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
        }
    }

    TargetObject->SetTransform(NewTransform);
}

void APivotTransformGizmo::OnMouseHover(const FVector<float> &RayOrigin, const FVector<float> &RayDir)
{
    // 드래그 중이거나 타겟이 없으면 Hover 처리를 생략합니다.
    if (bIsDragging || TargetObject == nullptr)
        return;

    TArray<UPrimitiveComponent *> *ActiveComponents = nullptr;

    switch (GizmoType)
    {
    case EGizmoHandleType::Translate:
        ActiveComponents = &TranslateGizmoComponents;
        break;
    case EGizmoHandleType::Rotate:
        ActiveComponents = &RotateGizmoComponents;
        break;
    case EGizmoHandleType::Scale:
        ActiveComponents = &ScaleGizmoComponents;
        break;
    }

    if (ActiveComponents == nullptr)
        return;

    float MinDistance = FLT_MAX;
    int   HitIndex = -1;

    for (int i = 0; i < ActiveComponents->size(); ++i)
    {
        UPrimitiveComponent *Comp = (*ActiveComponents)[i];
        if (Comp == nullptr)
            continue;

        FHitResult Hit = Comp->IntersectRay(RayOrigin, RayDir);
        if (Hit.bHit && Hit.Distance < MinDistance)
        {
            MinDistance = Hit.Distance;
            HitIndex = i;
        }
    }

    EGizmoAxis NewHoveredAxis = EGizmoAxis::None;
    if (HitIndex == 0)
        NewHoveredAxis = EGizmoAxis::X;
    else if (HitIndex == 1)
        NewHoveredAxis = EGizmoAxis::Y;
    else if (HitIndex == 2)
        NewHoveredAxis = EGizmoAxis::Z;

    // Hover된 축이 변경되었을 때만 색상 업데이트를 수행
    if (HoveredAxis != NewHoveredAxis)
    {
        HoveredAxis = NewHoveredAxis;
        UpdateColor();
    }
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

void APivotTransformGizmo::UpdateColor()
{
    auto ApplyColor = [&](TArray<UPrimitiveComponent *> &Comps)
    {
        if (Comps.size() < 3)
            return;

        // 기본 색상
        FVector4<float> ColorX = {1.0f, 0.0f, 0.0f, 1.0f};     // Red
        FVector4<float> ColorY = {0.0f, 1.0f, 0.0f, 1.0f};     // Green
        FVector4<float> ColorZ = {0.0f, 0.0f, 1.0f, 1.0f};     // Blue
        FVector4<float> HoverColor = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow

        // 드래그 중이라면 ActiveAxis를, 아니라면 HoveredAxis를 강조합니다.
        EGizmoAxis HighlightAxis = bIsDragging ? ActiveAxis : HoveredAxis;

        if (HighlightAxis == EGizmoAxis::X)
            ColorX = HoverColor;
        else if (HighlightAxis == EGizmoAxis::Y)
            ColorY = HoverColor;
        else if (HighlightAxis == EGizmoAxis::Z)
            ColorZ = HoverColor;

        if (Comps[0])
            Comps[0]->SetColor(ColorX);
        if (Comps[1])
            Comps[1]->SetColor(ColorY);
        if (Comps[2])
            Comps[2]->SetColor(ColorZ);
    };

    ApplyColor(TranslateGizmoComponents);
    ApplyColor(RotateGizmoComponents);
    ApplyColor(ScaleGizmoComponents);
}