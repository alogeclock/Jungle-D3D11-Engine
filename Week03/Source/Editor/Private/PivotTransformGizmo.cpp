#include "Source/Editor/Public/PivotTransformGizmo.h"
#include "Source/Core/Public/Memory.h"
#include "Source/Editor/Public/EditorViewportClient.h"
#include "Source/Engine/Public/GUI/ImGuiManager.h"

APivotTransformGizmo::APivotTransformGizmo(const FString& InString) : ABaseTransformGizmo(InString)
{
    USceneComponent* Root = new USceneComponent("PivotTransformSceneComponent");
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
    if (!GetGizmoTargetTransform(CenterTransform))
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
    for (auto* Comp : TranslateGizmoComponents)
        if (Comp)
            Comp->SetVisible(false);
    for (auto* Comp : RotateGizmoComponents)
        if (Comp)
            Comp->SetVisible(false);
    for (auto* Comp : ScaleGizmoComponents)
        if (Comp)
            Comp->SetVisible(false);

    if (!GEditor || !GEditor->GetSelection() || GEditor->GetSelection()->IsEmpty())
        return;

    if (GizmoType == EGizmoHandleType::Translate)
        for (auto* Comp : TranslateGizmoComponents)
            Comp->SetVisible(true);
    else if (GizmoType == EGizmoHandleType::Rotate)
        for (auto* Comp : RotateGizmoComponents)
            Comp->SetVisible(true);
    else if (GizmoType == EGizmoHandleType::Scale)
        for (auto* Comp : ScaleGizmoComponents)
            Comp->SetVisible(true);
}

bool APivotTransformGizmo::OnMouseDown(const FVector<float>& RayOrigin, const FVector<float>& RayDir)
{
    if (!GEditor || !GEditor->GetSelection() || GEditor->GetSelection()->IsEmpty())
        return false;

    // 가장 마지막으로 선택된 객체의 중심점을 기즈모의 초기 Transform으로 저장
    FTransform CenterTransform;
    if (!GetGizmoTargetTransform(CenterTransform))
        return false;

    InitialGizmoTransform = CenterTransform;

    // 선택된 모든 객체들의 컴포넌트와 초기 월드 Transform을 보관
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

    float MinDistance = FLT_MAX;
    int HitIndex = -1;

    for (int i = 0; i < ActiveComponents->size(); ++i)
    {
        UPrimitiveComponent* Comp = (*ActiveComponents)[i];
        if (Comp == nullptr)
            continue;

        FHitResult Hit = Comp->IntersectRay(RayOrigin, RayDir);
        if (Hit.bHit && Hit.Distance < MinDistance)
        {
            MinDistance = Hit.Distance;
            HitIndex = i;
        }
    }

    if (HitIndex == -1)
        return false;

    if (HitIndex == 0)
        ActiveAxis = EGizmoAxis::X;
    else if (HitIndex == 1)
        ActiveAxis = EGizmoAxis::Y;
    else if (HitIndex == 2)
        ActiveAxis = EGizmoAxis::Z;

    // [변경] InitialObjectTransform -> InitialGizmoTransform 으로 치환
    FMatrix<float> RotationMatrix = FRotationMatrix<float>(InitialGizmoTransform.Rotation);
    FVector4<float> Dir;

    if (ActiveAxis == EGizmoAxis::X)
        Dir = FVector4<float>(1.0f, 0.0f, 0.0f, 0.0f) * RotationMatrix;
    else if (ActiveAxis == EGizmoAxis::Y)
        Dir = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
    else if (ActiveAxis == EGizmoAxis::Z)
        Dir = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;

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
    }
    break;
    case EGizmoAxis::Y: {
        FVector4<float> Dir = FVector4<float>(0.0f, 1.0f, 0.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
    }
    break;
    case EGizmoAxis::Z: {
        FVector4<float> Dir = FVector4<float>(0.0f, 0.0f, 1.0f, 0.0f) * RotationMatrix;
        AxisDir = FVector<float>(Dir.X, Dir.Y, Dir.Z);
    }
    break;
    default:
        return;
    }

    AxisDir.Normalize();
    FTransform NewGizmoTransform = InitialGizmoTransform;

    if (GizmoType == EGizmoHandleType::Rotate)
    {
        FVector<float> PlaneNormal = AxisDir;
        float Denom = FVector<float>::DotProduct(RayDir, PlaneNormal);

        if (std::abs(Denom) < 0.001f)
            return;
        float t = FVector<float>::DotProduct(GizmoOrigin - RayOrigin, PlaneNormal) / Denom;
        if (t < 0.0f)
            return;

        FVector<float> HitPoint = RayOrigin + (RayDir * t);
        FVector<float> CurrentDragVector = HitPoint - GizmoOrigin;
        CurrentDragVector.Normalize();

        FVector<float> CrossProduct = FVector<float>::CrossProduct(InitialDragVector, CurrentDragVector);
        float y = FVector<float>::DotProduct(CrossProduct, PlaneNormal);
        float x = FVector<float>::DotProduct(InitialDragVector, CurrentDragVector);

        FMatrix<float> CurrentRotMat = FRotationMatrix<float>(InitialGizmoTransform.Rotation);
        float DeltaAngle = -std::atan2(y, x);

        FMatrix<float> DeltaRotMat;
        if (ActiveAxis == EGizmoAxis::X)
            DeltaRotMat = FRotationXMatrix<float>(DeltaAngle);
        else if (ActiveAxis == EGizmoAxis::Y)
            DeltaRotMat = FRotationYMatrix<float>(DeltaAngle);
        else if (ActiveAxis == EGizmoAxis::Z)
            DeltaRotMat = FRotationZMatrix<float>(DeltaAngle);

        FMatrix<float> NewRotMat = DeltaRotMat * CurrentRotMat;
        NewGizmoTransform.Rotation = NewRotMat.ToEuler();
    }
    else
    {
        float Denom = FVector<float>::DotProduct(RayDir, GizmoPlaneNormal);
        if (std::abs(Denom) < 0.001f)
            return;
        float t = FVector<float>::DotProduct(GizmoOrigin - RayOrigin, GizmoPlaneNormal) / Denom;
        if (t < 0.0f)
            return;

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

            if (ActiveAxis == EGizmoAxis::X)
                NewGizmoTransform.Scale.X += DeltaT * ScaleSensitivity;
            else if (ActiveAxis == EGizmoAxis::Y)
                NewGizmoTransform.Scale.Y += DeltaT * ScaleSensitivity;
            else if (ActiveAxis == EGizmoAxis::Z)
                NewGizmoTransform.Scale.Z += DeltaT * ScaleSensitivity;

            if (NewGizmoTransform.Scale.X < MinScale)
                NewGizmoTransform.Scale.X = MinScale;
            if (NewGizmoTransform.Scale.Y < MinScale)
                NewGizmoTransform.Scale.Y = MinScale;
            if (NewGizmoTransform.Scale.Z < MinScale)
                NewGizmoTransform.Scale.Z = MinScale;
        }
    }

    // ==========================================
    // [추가] 선택된 모든 객체에 상대적 Transform 일괄 적용 로직
    // ==========================================
    FMatrix<float> GizmoInitialRT = FRotationMatrix<float>(InitialGizmoTransform.Rotation) * FTranslationMatrix<float>(InitialGizmoTransform.Location);
    FMatrix<float> GizmoNewRT = FRotationMatrix<float>(NewGizmoTransform.Rotation) * FTranslationMatrix<float>(NewGizmoTransform.Location);
    FMatrix<float> GizmoInitialInvRT = GizmoInitialRT.Inverse();

    for (FSelectedObjectState& State : DraggingObjects)
    {
        FTransform ObjNewWorldTransform;

        if (GizmoType == EGizmoHandleType::Scale)
        {
            ObjNewWorldTransform.Scale = State.InitialWorldTransform.Scale * NewGizmoTransform.Scale;
            ObjNewWorldTransform.Rotation = State.InitialWorldTransform.Rotation;

            FVector<float> Offset = State.InitialWorldTransform.Location - InitialGizmoTransform.Location;
            FMatrix<float> GizmoRotMat = FRotationMatrix<float>(InitialGizmoTransform.Rotation);
            FMatrix<float> GizmoRotInv = GizmoRotMat.Inverse();
            
            FVector4<float> LocalOffset4 = FVector4<float>(Offset.X, Offset.Y, Offset.Z, 0.0f) * GizmoRotInv;
            FVector<float> LocalOffset(LocalOffset4.X, LocalOffset4.Y, LocalOffset4.Z);
            
            LocalOffset = LocalOffset * NewGizmoTransform.Scale;
            
            FVector4<float> ScaledOffset4 = FVector4<float>(LocalOffset.X, LocalOffset.Y, LocalOffset.Z, 0.0f) * GizmoRotMat;
            ObjNewWorldTransform.Location = InitialGizmoTransform.Location + FVector<float>(ScaledOffset4.X, ScaledOffset4.Y, ScaledOffset4.Z);
        }
        else
        {
            FMatrix<float> ObjInitialRT = FRotationMatrix<float>(State.InitialWorldTransform.Rotation) * FTranslationMatrix<float>(State.InitialWorldTransform.Location);
            FMatrix<float> ObjNewRT = ObjInitialRT * GizmoInitialInvRT * GizmoNewRT;

            ObjNewWorldTransform.Location = FVector<float>(ObjNewRT.M[3][0], ObjNewRT.M[3][1], ObjNewRT.M[3][2]);
            ObjNewWorldTransform.Rotation = ObjNewRT.ToEuler(); 
            ObjNewWorldTransform.Scale = State.InitialWorldTransform.Scale; // 스케일 원본 유지
        }

        // ==========================================
        // 부모-자식 계층 변환 (Local Transform으로 갱신)
        // ==========================================
        USceneComponent* ParentComp = State.Component->GetAttachParent();
        if (ParentComp)
        {
            // 부모의 변환 행렬에서도 스케일을 배제한 RT 행렬만 추출하여 로컬 변환 시의 팽이 버그를 막습니다.
            FTransform ParentTrans;
            ParentTrans = ParentTrans.ToTransform(ParentComp->GetWorldMatrix());
            
            FMatrix<float> ParentRT = FRotationMatrix<float>(ParentTrans.Rotation) * FTranslationMatrix<float>(ParentTrans.Location);
            FMatrix<float> ParentRTInv = ParentRT.Inverse();
            
            FMatrix<float> ObjNewWorldRT = FRotationMatrix<float>(ObjNewWorldTransform.Rotation) * FTranslationMatrix<float>(ObjNewWorldTransform.Location);
            FMatrix<float> ObjNewLocalRT = ObjNewWorldRT * ParentRTInv;
            
            FTransform FinalLocalTransform;
            FinalLocalTransform.Location = FVector<float>(ObjNewLocalRT.M[3][0], ObjNewLocalRT.M[3][1], ObjNewLocalRT.M[3][2]);
            FinalLocalTransform.Rotation = ObjNewLocalRT.ToEuler();
            FinalLocalTransform.Scale = ObjNewWorldTransform.Scale; // 로컬 스케일 유지
            
            State.Component->SetTransform(FinalLocalTransform);
        }
        else
        {
            State.Component->SetTransform(ObjNewWorldTransform);
        }
    }
}

void APivotTransformGizmo::OnMouseHover(const FVector<float>& RayOrigin, const FVector<float>& RayDir)
{
    if (bIsDragging)
        return;
    if (!GEditor || !GEditor->GetSelection() || GEditor->GetSelection()->IsEmpty())
        return;

    TArray<UPrimitiveComponent*>* ActiveComponents = nullptr;

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
    int HitIndex = -1;

    for (int i = 0; i < ActiveComponents->size(); ++i)
    {
        UPrimitiveComponent* Comp = (*ActiveComponents)[i];
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
    if (!GetGizmoTargetTransform(CenterTransform))
        return;

    uint32 CurrentModeIndex = static_cast<uint32>(GizmoType);
    uint32 NextModeIndex = (CurrentModeIndex + 1) % 3;
    GizmoType = static_cast<EGizmoHandleType>(NextModeIndex);

    UpdateVisibility();
}

void APivotTransformGizmo::UpdateColor()
{
    // ... (기존 UpdateColor 유지)
    auto ApplyColor = [&](TArray<UPrimitiveComponent*>& Comps) {
        if (Comps.size() < 3)
            return;

        FVector4<float> ColorX = {1.0f, 0.0f, 0.0f, 1.0f};
        FVector4<float> ColorY = {0.0f, 1.0f, 0.0f, 1.0f};
        FVector4<float> ColorZ = {0.0f, 0.0f, 1.0f, 1.0f};
        FVector4<float> HoverColor = {1.0f, 1.0f, 0.0f, 1.0f};

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

    FVector4<float> TargetWorldPos(TargetTransform.Location.X, TargetTransform.Location.Y, TargetTransform.Location.Z,
                                   1.0f);
    FVector4<float> TargetViewPos = TargetWorldPos * ViewMatrix;

    float Distance = std::abs(TargetViewPos.Z);
    if (Distance < 0.01f)
        Distance = 0.01f;

    float ScaleFactor = 1.0f;

    if (!bIsOrtho)
    {
        float ZDepth = std::abs(TargetViewPos.Z);
        if (ZDepth < 0.01f)
            ZDepth = 0.01f;

        float EuclideanDist = std::sqrt(TargetViewPos.X * TargetViewPos.X + TargetViewPos.Y * TargetViewPos.Y +
                                        TargetViewPos.Z * TargetViewPos.Z);
        if (EuclideanDist < 0.01f)
            EuclideanDist = 0.01f;

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

// Selection 배열 안의 실제 조작 타겟 컴포넌트들을 모두 추출하는 유틸리티
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

// 마지막으로 선택한 컴포넌트를 기준으로 중심점 도출
bool APivotTransformGizmo::GetGizmoTargetTransform(FTransform& OutTransform)
{
    USelection* Selection = GEditor->GetSelection();
    if (!Selection || Selection->IsEmpty())
        return false;

    // Selection의 마지막 인덱스에 있는 객체가 '가장 마지막에 선택된 객체'입니다.
    uint32 LastIndex = Selection->GetCount() - 1;
    UObject* TargetObj = Selection->GetSelectedObject(LastIndex);

    USceneComponent* PivotComp = nullptr;

    if (AActor* SelectedActor = Cast<AActor>(TargetObj))
    {
        PivotComp = SelectedActor->GetRootComponent();
    }
    else if (USceneComponent* SelectedComp = Cast<USceneComponent>(TargetObj))
    {
        PivotComp = SelectedComp;
    }

    if (!PivotComp)
        return false;

    FTransform TempTrans;
    FTransform WorldTrans = TempTrans.ToTransform(PivotComp->GetWorldMatrix());

    OutTransform.Location = WorldTrans.Location;
    OutTransform.Rotation = WorldTrans.Rotation;
    OutTransform.Scale = FVector<float>(1.0f, 1.0f, 1.0f); // 기즈모 시각적 표시용이므로 1.0 고정

    return true;
}