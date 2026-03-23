#include "Source/Core/Public/Memory.h"
#include "Source/Editor/Public/PivotTransformGizmo.h"
#include "Source/Engine/Public/GUI/ImGuiManager.h"

#include "Source/Editor/Public/EditorViewportClient.h"

APivotTransformGizmo::APivotTransformGizmo(const FString &InString) : ABaseTransformGizmo(InString)
{
    USceneComponent *Root = new USceneComponent("PivotTransformSceneComponent");
    this->SetRootComponent(Root);
    AddOwnedComponent(Root);
    Root->RegisterComponent();

    const float HALF_PI = 1.570796f;

    struct FGizmoAxisInfo
    {
        const char* Prefix;
        FVector<float> Rotation;
        FVector4<float> Color;
    };

    FGizmoAxisInfo Axes[3] = {
        { "X", {0.0f, -HALF_PI, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f} },
        { "Y", {HALF_PI, 0.0f, 0.0f},  {0.0f, 1.0f, 0.0f, 1.0f} },
        { "Z", {0.0f, 0.0f, 0.0f},     {0.0f, 0.0f, 1.0f, 1.0f} }
    };

    const char* ComponentSuffixes[3] = { "ArrowComponent", "RingComponent", "CubeArrowComponent" };

    for (int Mode = 0; Mode < 3; ++Mode)
    {
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            FString CompName = FString(Axes[Axis].Prefix) + ComponentSuffixes[Mode];
            UPrimitiveComponent* Comp = nullptr;

            if (Mode == 0)
            {
                auto Arrow = new UArrowComponent(CompName);
                TranslateGizmoComponents.push_back(Arrow);
                Comp = Arrow;
            }
            else if (Mode == 1)
            {
                auto Ring = new URingComponent(CompName);
                RotateGizmoComponents.push_back(Ring);
                Comp = Ring;
            }
            else if (Mode == 2)
            {
                auto CubeArrow = new UCubeArrowComponent(CompName);
                ScaleGizmoComponents.push_back(CubeArrow);
                Comp = CubeArrow;
            }

            // 공통 속성 일괄 설정
            if (Comp)
            {
                Comp->SetOuter(this);
                //AddOwnedComponent(Comp);
                Comp->RegisterComponent();

                Comp->SetIsInEditor(true);
                Comp->SetRotation(Axes[Axis].Rotation);
                Comp->SetColor(Axes[Axis].Color);
                Comp->SetCullMode(ECullMode::None);
                Comp->SetEnableDepthTest(false);
                Comp->SetVisible(true);
            }
        }
    }

    GizmoType = EGizmoHandleType::Translate;
    UpdateVisibility();
}

APivotTransformGizmo::~APivotTransformGizmo()
{
    TranslateGizmoComponents.clear();
    RotateGizmoComponents.clear();
    ScaleGizmoComponents.clear();
}

void APivotTransformGizmo::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime); // 부모 클래스의 틱(컴포넌트 틱 등) 실행
    UpdateVisibility();
    UpdateColor();

    if (TargetObject == nullptr)
        return;

    // 뷰포트와 카메라 정보를 능동적으로 가져옵니다.
    FEditorViewportClient* ViewportClient = UImGuiManager::Get().GetEditorViewportClient();
    FViewportCameraTransform *Camera = UImGuiManager::Get().GetCamera();
    if (!ViewportClient || !Camera) 
        return;

    FMatrix<float> ViewMatrix = ViewportClient->GetViewMatrix();
    float FOV = Camera->GetFOV();
    bool bIsOrtho = UImGuiManager::Get().bIsOrthogonal;
    
    float OrthoWidth = 10.0f;
    if (bIsOrtho)
    {
        FVector<float> dir = Camera->GetLocation() - Camera->GetLookAt();
        OrthoWidth = dir.Length() * 2.0f;
    }

    FTransform TargetTransform = TargetObject->GetTransform();
    FTransform UnscaledTransform;

    UnscaledTransform.Location = TargetTransform.Location;
    UnscaledTransform.Rotation = TargetTransform.Rotation;
    
    // 타겟의 3D 월드 위치를 Vector4로 구성한 뒤, ViewMatrix를 곱해 카메라 기준 로컬 좌표로 변환한다.
    // 이 떄의 Z값이 곧 물체부터 카메라까지의 정확한 깊이이다.
    FVector4<float> TargetWorldPos(TargetTransform.Location.X, TargetTransform.Location.Y, TargetTransform.Location.Z, 1.0f);
    FVector4<float> TargetViewPos = TargetWorldPos * ViewMatrix;

    float Distance = std::abs(TargetViewPos.Z);
    if (Distance < 0.01f) Distance = 0.01f; // 거리가 너무 가까울 때 사라짐 방지

    float ScaleFactor = 1.0f;

    if (!bIsOrtho)
    {        
        // 1. 카메라 평면으로부터의 깊이 (Z Depth)
        float ZDepth = std::abs(TargetViewPos.Z);
        if (ZDepth < 0.01f) ZDepth = 0.01f; 
        
        // 1. 카메라 원점으로부터 기즈모까지의 '실제 3D 직선 거리(Euclidean Distance)' 계산
        float EuclideanDist = std::sqrt(TargetViewPos.X * TargetViewPos.X + 
                                        TargetViewPos.Y * TargetViewPos.Y + 
                                        TargetViewPos.Z * TargetViewPos.Z);
        if (EuclideanDist < 0.01f) EuclideanDist = 0.01f;
        
        // 3. 화면 중심(Z축)에서 벗어난 각도의 Cosine 값 도출 (Z / Distance)
        float CosineAngle = ZDepth / EuclideanDist;

        // 4. ZDepth에 CosineAngle을 한 번 더 곱해줌 (결과적으로 Z^2 / EuclideanDist)
        float CorrectedDistance = ZDepth * CosineAngle;
        float FOVRad = FOV * (3.14159265f / 180.0f);
        float FOVScale = std::tan(FOVRad / 2.0f);

        // 최종 스케일은 보정된 거리와 FOV 스케일을 곱하여 결정
        ScaleFactor *= CorrectedDistance * FOVScale * 25.0f;
    }
    else
    {
        ScaleFactor = OrthoWidth * 0.2f;
    }

    UnscaledTransform.Scale = FVector<float>(ScaleFactor, ScaleFactor, ScaleFactor);
    this->SetTransform(UnscaledTransform);
}

void APivotTransformGizmo::UpdateVisibility()
{
    // 1. 모든 기즈모 파츠 숨기기
    for (auto* Comp : TranslateGizmoComponents) if(Comp) Comp->SetVisible(false);
    for (auto* Comp : RotateGizmoComponents) if(Comp) Comp->SetVisible(false);
    for (auto* Comp : ScaleGizmoComponents) if(Comp) Comp->SetVisible(false);

    if (TargetObject == nullptr)
        return;

    // 2. 현재 모드에 해당하는 파츠만 보이게 켜기
    if (GizmoType == EGizmoHandleType::Translate)
        for (auto* Comp : TranslateGizmoComponents) Comp->SetVisible(true);
    else if (GizmoType == EGizmoHandleType::Rotate)
        for (auto* Comp : RotateGizmoComponents) Comp->SetVisible(true);
    else if (GizmoType == EGizmoHandleType::Scale)
        for (auto* Comp : ScaleGizmoComponents) Comp->SetVisible(true);
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

        // 정확한 부호와 각도를 얻기 위해 atan2 사용
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
    UpdateColor();
}

void APivotTransformGizmo::ToggleMode()
{
    if (TargetObject == nullptr)
        return;

    uint32 CurrentModeIndex = static_cast<uint32>(GizmoType);
    uint32 NextModeIndex = (CurrentModeIndex + 1) % 3;
    GizmoType = static_cast<EGizmoHandleType>(NextModeIndex);
    
    UpdateVisibility();
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