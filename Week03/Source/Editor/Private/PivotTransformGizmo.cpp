#include "Source/Editor/Public/PivotTransformGizmo.h"
#include "Source/Engine/Public/GUI/ImGuiManager.h"
#include "Source/Core/Public/Memory.h"
#include "Source/Editor/Public/EditorViewportClient.h"

APivotTransformGizmo::APivotTransformGizmo(const FString& InString) : ABaseTransformGizmo(InString)
{
    USceneComponent* Root = new USceneComponent("PivotTransformSceneComponent");
    this->SetRootComponent(Root);
    AddOwnedComponent(Root);
    Root->RegisterComponent();

    const float HALF_PI = 1.570796f;

    struct FGizmoAxisInfo {
        const char* Prefix;
        FVector<float> Rotation;
        FVector4<float> Color;
    };

    FGizmoAxisInfo Axes[3] = {
        {"X", {0.0f, -HALF_PI, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {"Y",  {HALF_PI, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {"Z",     {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
    };

    const char* ComponentSuffixes[3] = {"ArrowComponent", "RingComponent", "CubeArrowComponent"};

    for (int Mode = 0; Mode < 3; ++Mode)
    {
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            FString CompName = FString(Axes[Axis].Prefix) + ComponentSuffixes[Mode];
            UPrimitiveComponent* Comp = nullptr;

            if (Mode == 0) {
                auto Arrow = new UArrowComponent(CompName);
                TranslateGizmoComponents.push_back(Arrow);
                Comp = Arrow;
            } else if (Mode == 1) {
                auto Ring = new URingComponent(CompName);
                RotateGizmoComponents.push_back(Ring);
                Comp = Ring;
            } else if (Mode == 2) {
                auto CubeArrow = new UCubeArrowComponent(CompName);
                ScaleGizmoComponents.push_back(CubeArrow);
                Comp = CubeArrow;
            }

            if (Comp)
            {
                Comp->SetOuter(this);
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
    AActor::Tick(DeltaTime);

    if (!GEditor || !GEditor->GetSelection() || GEditor->GetSelection()->IsEmpty())
    {
        UpdateVisibility();
        return;
    }

    FTransform CenterTransform;
    if (!GetGizmoCenterTransform(CenterTransform))
    {
        UpdateVisibility();
        return;
    }

    UpdateVisibility();
    UpdateColor();

    FTransform UnscaledTransform = CalculateUnscaledTransform(CenterTransform);
    this->SetTransform(UnscaledTransform);
}

void APivotTransformGizmo::UpdateVisibility()
{
    // ... (기존 UpdateVisibility 유지)
    for (auto* Comp : TranslateGizmoComponents) if (Comp) Comp->SetVisible(false);
    for (auto* Comp : RotateGizmoComponents) if (Comp) Comp->SetVisible(false);
    for (auto* Comp : ScaleGizmoComponents) if (Comp) Comp->SetVisible(false);

    if (!GEditor || !GEditor->GetSelection() || GEditor->GetSelection()->IsEmpty()) return;

    if (GizmoType == EGizmoHandleType::Translate)
        for (auto* Comp : TranslateGizmoComponents) Comp->SetVisible(true);
    else if (GizmoType == EGizmoHandleType::Rotate)
        for (auto* Comp : RotateGizmoComponents) Comp->SetVisible(true);
    else if (GizmoType == EGizmoHandleType::Scale)
        for (auto* Comp : ScaleGizmoComponents) Comp->SetVisible(true);
}

bool APivotTransformGizmo::OnMouseDown(const FVector<float>& RayOrigin, const FVector<float>& RayDir)
{
    if (!GEditor || !GEditor->GetSelection() || GEditor->GetSelection()->IsEmpty())
        return false;

    // [변경] 다중 선택 객체의 중심점을 기즈모의 초기 Transform으로 저장
    FTransform CenterTransform;
    if (!GetGizmoCenterTransform(CenterTransform))
        return false;

    InitialGizmoTransform = CenterTransform;

    // [추가] 선택된 모든 객체들의 컴포넌트와 초기 월드 Transform을 보관
    DraggingObjects.clear();
    TArray<USceneComponent*> TargetComps;
    GetTargetComponents(TargetComps);

    for (USceneComponent* Comp : TargetComps)
    {
        FSelectedObjectState State;
        State.Component = Comp;
        FTransform TempTrans;
        State.InitialWorldTransform = TempTrans.ToTransform(Comp->GetWorldMatrix());
        DraggingObjects.push_back(State);
    }

    std::vector<UPrimitiveComponent*>* ActiveComponents = nullptr;
    switch (GizmoType)
    {
    case EGizmoHandleType::Translate: ActiveComponents = &TranslateGizmoComponents; break;
    case EGizmoHandleType::Rotate: ActiveComponents = &RotateGizmoComponents; break;
    case EGizmoHandleType::Scale: ActiveComponents = &ScaleGizmoComponents; break;
    }

    if (ActiveComponents == nullptr) return false;

    float MinDistance = FLT_MAX;
    int HitIndex = -1;

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

    if (HitIndex == -1) return false;

    if (HitIndex == 0) ActiveAxis = EGizmoAxis::X;
    else if (HitIndex == 1) ActiveAxis = EGizmoAxis::Y;
    else if (HitIndex == 2) ActiveAxis = EGizmoAxis::Z;

    // [변경] InitialObjectTransform -> InitialGizmoTransform 으로 치환
    FMatrix<float> RotationMatrix = FRotationMatrix<float>(InitialGizmoTransform.Rotation);
    FVector4<float> Dir;

    if (ActiveAxis == EGizmoAxis::X) Dir = FVector4<float>(1.0f, 0.0f, 0.0f, 0.0f) * RotationMatrix;
    else if (ActiveAxis == EGizmoAxis::Y) Dir = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
    else if (ActiveAxis == EGizmoAxis::Z) Dir = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;

    FVector<float> AxisDir(Dir.X, Dir.Y, Dir.Z);
    AxisDir.Normalize();

    FVector<float> CameraDir = RayDir; 
    GizmoPlaneNormal = FVector<float>(-RayDir.X, -RayDir.Y, -RayDir.Z);
    GizmoPlaneNormal.Normalize();

    if (GizmoType == EGizmoHandleType::Rotate)
    {
        GizmoPlaneNormal = AxisDir;
        float Denom = FVector<float>::DotProduct(RayDir, AxisDir);
        if (std::abs(Denom) > 0.001f)
        {
            float t = FVector<float>::DotProduct(InitialGizmoTransform.Location - RayOrigin, AxisDir) / Denom;
            FVector<float> HitPoint = RayOrigin + (RayDir * t);
            InitialDragVector = HitPoint - InitialGizmoTransform.Location;
            InitialDragVector.Normalize();
        }
    }
    else
    {
        float Denom = FVector<float>::DotProduct(RayDir, GizmoPlaneNormal);
        if (std::abs(Denom) > 0.0001f)
        {
            float t = FVector<float>::DotProduct(InitialGizmoTransform.Location - RayOrigin, GizmoPlaneNormal) / Denom;
            FVector<float> HitPoint = RayOrigin + (RayDir * t);
            InitialRayDistance = FVector<float>::DotProduct(HitPoint - InitialGizmoTransform.Location, AxisDir);
        }
        else
        {
            InitialRayDistance = 0.0f;
        }
    }

    bIsDragging = true;
    return true;
}

void APivotTransformGizmo::OnMouseMove(const FVector<float>& RayOrigin, const FVector<float>& RayDir)
{
    if (!GEditor || !GEditor->GetSelection() || GEditor->GetSelection()->IsEmpty() || DraggingObjects.empty())
        return;

    FVector<float> GizmoOrigin = InitialGizmoTransform.Location;
    FVector<float> AxisDir;

    FMatrix<float> RotationMatrix = FRotationMatrix<float>(InitialGizmoTransform.Rotation);
    switch (ActiveAxis)
    {
    case EGizmoAxis::X: {
        FVector4<float> Dir = FVector4<float>(1.0f, 0.0f, 0.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
    } break;
    case EGizmoAxis::Y: {
        FVector4<float> Dir = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
    } break;
    case EGizmoAxis::Z: {
        FVector4<float> Dir = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
    } break;
    default: return;
    }

    AxisDir.Normalize();
    FTransform NewGizmoTransform = InitialGizmoTransform;

    if (GizmoType == EGizmoHandleType::Rotate)
    {
        FVector<float> PlaneNormal = AxisDir;
        float Denom = FVector<float>::DotProduct(RayDir, PlaneNormal);

        if (std::abs(Denom) < 0.001f) return;
        float t = FVector<float>::DotProduct(GizmoOrigin - RayOrigin, PlaneNormal) / Denom;
        if (t < 0.0f) return;

        FVector<float> HitPoint = RayOrigin + (RayDir * t);
        FVector<float> CurrentDragVector = HitPoint - GizmoOrigin;
        CurrentDragVector.Normalize();

        FVector<float> CrossProduct = FVector<float>::CrossProduct(InitialDragVector, CurrentDragVector);
        float y = FVector<float>::DotProduct(CrossProduct, PlaneNormal); 
        float x = FVector<float>::DotProduct(InitialDragVector, CurrentDragVector); 

        FMatrix<float> CurrentRotMat = FRotationMatrix<float>(InitialGizmoTransform.Rotation);
        float DeltaAngle = -std::atan2(y, x);

        FMatrix<float> DeltaRotMat;
        if (ActiveAxis == EGizmoAxis::X) DeltaRotMat = FRotationXMatrix<float>(DeltaAngle);
        else if (ActiveAxis == EGizmoAxis::Y) DeltaRotMat = FRotationYMatrix<float>(DeltaAngle);
        else if (ActiveAxis == EGizmoAxis::Z) DeltaRotMat = FRotationZMatrix<float>(DeltaAngle);

        FMatrix<float> NewRotMat = DeltaRotMat * CurrentRotMat;
        NewGizmoTransform.Rotation = NewRotMat.ToEuler();
    }
    else
    {
        float Denom = FVector<float>::DotProduct(RayDir, GizmoPlaneNormal);
        if (std::abs(Denom) < 0.001f) return;
        float t = FVector<float>::DotProduct(GizmoOrigin - RayOrigin, GizmoPlaneNormal) / Denom;
        if (t < 0.0f) return;

        FVector<float> HitPoint = RayOrigin + (RayDir * t);
        float AxisT = FVector<float>::DotProduct(HitPoint - GizmoOrigin, AxisDir);
        float DeltaT = AxisT - InitialRayDistance;

        if (GizmoType == EGizmoHandleType::Translate)
        {
            NewGizmoTransform.Location = InitialGizmoTransform.Location + (AxisDir * DeltaT);
        }
        else if (GizmoType == EGizmoHandleType::Scale)
        {
            const float ScaleSensitivity = 0.2f;
            const float MinScale = 0.01f;

            if (ActiveAxis == EGizmoAxis::X) NewGizmoTransform.Scale.X += DeltaT * ScaleSensitivity;
            else if (ActiveAxis == EGizmoAxis::Y) NewGizmoTransform.Scale.Y += DeltaT * ScaleSensitivity;
            else if (ActiveAxis == EGizmoAxis::Z) NewGizmoTransform.Scale.Z += DeltaT * ScaleSensitivity;

            if (NewGizmoTransform.Scale.X < MinScale) NewGizmoTransform.Scale.X = MinScale;
            if (NewGizmoTransform.Scale.Y < MinScale) NewGizmoTransform.Scale.Y = MinScale;
            if (NewGizmoTransform.Scale.Z < MinScale) NewGizmoTransform.Scale.Z = MinScale;
        }
    }

    // ==========================================
    // [추가] 선택된 모든 객체에 상대적 Transform 일괄 적용 로직
    // ==========================================
    FMatrix<float> GizmoInitialWorldMat = InitialGizmoTransform.ToMatrix();
    FMatrix<float> GizmoNewWorldMat = NewGizmoTransform.ToMatrix();
    FMatrix<float> GizmoInitialInv = GizmoInitialWorldMat.Inverse();

    for (FSelectedObjectState& State : DraggingObjects)
    {
        FMatrix<float> ObjInitialWorldMat = State.InitialWorldTransform.ToMatrix();
        
        // 1. 객체를 기준점(초기 기즈모) 좌표계로 로컬 변환 후, 새로운 기즈모 월드 행렬을 곱해 갱신된 월드 좌표계 도출
        FMatrix<float> ObjNewWorldMat = ObjInitialWorldMat * GizmoInitialInv * GizmoNewWorldMat;

        FTransform ObjNewTransform;
        ObjNewTransform = ObjNewTransform.ToTransform(ObjNewWorldMat);

        // 2. 부모-자식 계층이 있는 경우 Local Transform으로 재변환
        USceneComponent* ParentComp = State.Component->GetAttachParent();
        if (ParentComp)
        {
            FMatrix<float> ParentWorldInv = ParentComp->GetWorldMatrix().Inverse();
            FMatrix<float> ObjNewLocalMat = ObjNewWorldMat * ParentWorldInv;
            ObjNewTransform = ObjNewTransform.ToTransform(ObjNewLocalMat);
        }

        // 3. 최종 Transform 적용
        State.Component->SetTransform(ObjNewTransform);
    }
}

void APivotTransformGizmo::OnMouseHover(const FVector<float>& RayOrigin, const FVector<float>& RayDir)
{
    if (bIsDragging) return;
    if (!GEditor || !GEditor->GetSelection() || GEditor->GetSelection()->IsEmpty()) return;

    TArray<UPrimitiveComponent*>* ActiveComponents = nullptr;

    switch (GizmoType)
    {
    case EGizmoHandleType::Translate: ActiveComponents = &TranslateGizmoComponents; break;
    case EGizmoHandleType::Rotate: ActiveComponents = &RotateGizmoComponents; break;
    case EGizmoHandleType::Scale: ActiveComponents = &ScaleGizmoComponents; break;
    }

    if (ActiveComponents == nullptr) return;

    float MinDistance = FLT_MAX;
    int HitIndex = -1;

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

    EGizmoAxis NewHoveredAxis = EGizmoAxis::None;
    if (HitIndex == 0) NewHoveredAxis = EGizmoAxis::X;
    else if (HitIndex == 1) NewHoveredAxis = EGizmoAxis::Y;
    else if (HitIndex == 2) NewHoveredAxis = EGizmoAxis::Z;

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
    DraggingObjects.clear(); // 드래그 종료 시 상태 비우기
    UpdateColor();
}

bool APivotTransformGizmo::OnKeyDown(const FKey& Key)
{
    if (Key == EKeys::Space)
    {
        ToggleMode();
        return true;
    }
    return false;
}

void APivotTransformGizmo::ToggleMode()
{
    FTransform CenterTransform;
    if (!GetGizmoCenterTransform(CenterTransform))
        return;

    uint32 CurrentModeIndex = static_cast<uint32>(GizmoType);
    uint32 NextModeIndex = (CurrentModeIndex + 1) % 3;
    GizmoType = static_cast<EGizmoHandleType>(NextModeIndex);

    UpdateVisibility();
}

void APivotTransformGizmo::UpdateColor()
{
    // ... (기존 UpdateColor 유지)
    auto ApplyColor = [&](TArray<UPrimitiveComponent *> &Comps)
    {
        if (Comps.size() < 3) return;

        FVector4<float> ColorX = {1.0f, 0.0f, 0.0f, 1.0f};     
        FVector4<float> ColorY = {0.0f, 1.0f, 0.0f, 1.0f};     
        FVector4<float> ColorZ = {0.0f, 0.0f, 1.0f, 1.0f};     
        FVector4<float> HoverColor = {1.0f, 1.0f, 0.0f, 1.0f}; 

        EGizmoAxis HighlightAxis = bIsDragging ? ActiveAxis : HoveredAxis;

        if (HighlightAxis == EGizmoAxis::X) ColorX = HoverColor;
        else if (HighlightAxis == EGizmoAxis::Y) ColorY = HoverColor;
        else if (HighlightAxis == EGizmoAxis::Z) ColorZ = HoverColor;

        if (Comps[0]) Comps[0]->SetColor(ColorX);
        if (Comps[1]) Comps[1]->SetColor(ColorY);
        if (Comps[2]) Comps[2]->SetColor(ColorZ);
    };

    ApplyColor(TranslateGizmoComponents);
    ApplyColor(RotateGizmoComponents);
    ApplyColor(ScaleGizmoComponents);
}

FTransform APivotTransformGizmo::CalculateUnscaledTransform(FTransform TargetTransform)
{
    FTransform UnscaledTransform;
    UnscaledTransform.Location = TargetTransform.Location;
    UnscaledTransform.Rotation = TargetTransform.Rotation;

    FEditorViewportClient* ViewportClient = GEditor->GetEditorViewportClient();
    FViewportCameraTransform Camera = ViewportClient->GetCameraTransform();

    FMatrix<float> ViewMatrix = ViewportClient->GetViewMatrix();
    float FOV = Camera.GetFOV();
    bool bIsOrtho = UImGuiManager::Get().bIsOrthogonal;

    float OrthoWidth = 10.0f;
    if (bIsOrtho)
    {
        FVector<float> dir = Camera.GetLocation() - Camera.GetLookAt();
        OrthoWidth = dir.Length() * 2.0f;
    }

    FVector4<float> TargetWorldPos(TargetTransform.Location.X, TargetTransform.Location.Y, TargetTransform.Location.Z, 1.0f);
    FVector4<float> TargetViewPos = TargetWorldPos * ViewMatrix;

    float Distance = std::abs(TargetViewPos.Z);
    if (Distance < 0.01f) Distance = 0.01f; 

    float ScaleFactor = 1.0f;

    if (!bIsOrtho)
    {
        float ZDepth = std::abs(TargetViewPos.Z);
        if (ZDepth < 0.01f) ZDepth = 0.01f;

        float EuclideanDist = std::sqrt(TargetViewPos.X * TargetViewPos.X + TargetViewPos.Y * TargetViewPos.Y + TargetViewPos.Z * TargetViewPos.Z);
        if (EuclideanDist < 0.01f) EuclideanDist = 0.01f;

        float CosineAngle = ZDepth / EuclideanDist;
        float CorrectedDistance = ZDepth * CosineAngle;
        float FOVRad = FOV * (3.14159265f / 180.0f);
        float FOVScale = std::tan(FOVRad / 2.0f);

        ScaleFactor *= CorrectedDistance * FOVScale * 25.0f;
    }
    else
    {
        ScaleFactor = OrthoWidth * 0.2f;
    }

    UnscaledTransform.Scale = FVector<float>(ScaleFactor, ScaleFactor, ScaleFactor);
    return UnscaledTransform;
}

// [추가] Selection 배열 안의 실제 조작 타겟 컴포넌트들을 모두 추출하는 유틸리티
void APivotTransformGizmo::GetTargetComponents(TArray<USceneComponent*>& OutComponents)
{
    USelection* Selection = GEditor->GetSelection();
    for (uint32 i = 0; i < Selection->GetCount(); ++i)
    {
        UObject* TargetObj = Selection->GetSelectedObject(i);
        if (AActor* SelectedActor = Cast<AActor>(TargetObj))
        {
            if (SelectedActor->GetRootComponent())
                OutComponents.push_back(SelectedActor->GetRootComponent());
        }
        else if (USceneComponent* SelectedComp = Cast<USceneComponent>(TargetObj))
        {
            OutComponents.push_back(SelectedComp);
        }
    }
}

// [추가] 선택된 모든 컴포넌트들의 위치를 평균내어 다중 선택 기준(Center Transform)을 도출
bool APivotTransformGizmo::GetGizmoCenterTransform(FTransform& OutTransform)
{
    TArray<USceneComponent*> TargetComps;
    GetTargetComponents(TargetComps);

    if (TargetComps.empty())
        return false;

    FVector<float> CenterPos(0.0f, 0.0f, 0.0f);
    for (USceneComponent* Comp : TargetComps)
    {
        FTransform TempTrans;
        FTransform WorldTrans = TempTrans.ToTransform(Comp->GetWorldMatrix());
        CenterPos += WorldTrans.Location;
    }
    CenterPos = CenterPos / static_cast<float>(TargetComps.size());

    // 기즈모의 축 자체는 첫 번째 선택된 객체의 회전을 따르도록 설정 (또는 필요에 따라 월드 축(0,0,0)으로 변경 가능)
    FTransform TempTransFirst;
    FTransform FirstObjTrans = TempTransFirst.ToTransform(TargetComps[0]->GetWorldMatrix());
    
    OutTransform.Location = CenterPos;
    OutTransform.Rotation = FirstObjTrans.Rotation;
    OutTransform.Scale = FVector<float>(1.0f, 1.0f, 1.0f); // 기즈모 표시 스케일은 오버라이드 되므로 1로 설정

    return true;
}

// GetAnchorComponent() 함수는 더 이상 사용되지 않으므로 삭제합니다.