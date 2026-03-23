#include "Source/Editor/Public/EditorViewportClient.h"
#include "Source/Core/Public/Math/ViewMatrix.h"
#include "Source/Core/Public/Memory.h"
#include "Source/Engine/Object/Public/Actor.h"
#include "World.h"

#include <iostream>

FViewportCameraTransform::FViewportCameraTransform()
    : ViewLocation(-2.0f, 0.5f, 2.f), ViewRotation(-30.f, 0.f, 0.f), LookAt(0.f, 0.f, 0.f), OrthoZoom(1.f), Max_OrthoZoom(1000.f), Min_OrthoZoom(1.f),
      MaxLocation(1000.0)
{
}

void FViewportCameraTransform::SetLocation(const FVector<float> &Position)
{
    auto Clamp = [&](float V) -> float
    {
        float MaxBoundary = static_cast<float>(MaxLocation);
        if (V < -MaxBoundary)
            return -MaxBoundary;
        else if (V > MaxBoundary)
            return MaxBoundary;
        return V;
    };
    ViewLocation = {Clamp(Position.X), Clamp(Position.Y), Clamp(Position.Z)};
}

FMatrix<float> FViewportCameraTransform::ComputeOrbitMatrix() const
{
    FVector<float> worldUp = {0.0f, 0.0f, 1.0f};
    const float    PI = 3.141592f;

    // 1. ViewRotation을 바탕으로 실제 카메라가 바라보는 Forward 벡터 계산
    const float PitchRad = ViewRotation.X * (PI / 180.f);
    const float YawRad = ViewRotation.Y * (PI / 180.f);

    FVector<float> Forward = {std::cos(PitchRad) * std::cos(YawRad), std::cos(PitchRad) * std::sin(YawRad), std::sin(PitchRad)};

    // 2. 카메라 위치(ViewLocation)에서 Forward 방향으로 뻗어나간 지점을 새로운 LookAt으로 설정
    FVector<float> DynamicLookAt = {ViewLocation.X + Forward.X, ViewLocation.Y + Forward.Y, ViewLocation.Z + Forward.Z};

    // 3. 동적으로 생성된 LookAt을 ViewMatrix에 적용
    return FViewMatrix(ViewLocation, DynamicLookAt, worldUp);
}

FEditorViewportClient::FEditorViewportClient(FViewport *viewport)
{
    Viewport = viewport;
    Gizmo = nullptr;
    Axis = nullptr;
    Grid = nullptr;

    UImGuiManager::Get().SetCamera(&CameraTransform);
    UImGuiManager::Get().SetEditorViewportClient(this);

    LoadConfig();
}

FEditorViewportClient::~FEditorViewportClient()
{
    SaveConfig();
}

void FEditorViewportClient::Tick(float DeltaTime, FViewport *Viewport)
{
    if (!Viewport)
        return;

    ApplyMovement(DeltaTime, Viewport);

    if (UImGuiManager::Get().bToggleGizmoMode)
    {
        Gizmo->ToggleMode();
        UImGuiManager::Get().bToggleGizmoMode = false;
    }
}

bool FEditorViewportClient::InputKey(const FInputEventState &InputState)
{
    const EInputEvent Event = InputState.GetInputEvent();
    const FKey        Key = InputState.GetKey();

    if (Key == EKeys::LeftMouseButton)
    {
        if (Event == EInputEvent::Pressed)
        {
            if (UImGuiManager::Get().IsCaptureMouse())
                return true;

            bLeftMouseDragging = true;
            FRay ray = GetPickingRay();

            // 기즈모의 축(화살표)과 마우스 Ray가 충돌했는지 검사한 뒤, 기즈모를 클릭했다면 뒤의 씬 객체 피킹은
            // 생략한다.
            if (Gizmo != nullptr && Gizmo->GetTargetObject() != nullptr)
                if (Gizmo->OnMouseDown(ray.Origin, ray.Direction))
                    return true;

            PickingRay(ray.Origin, ray.Direction);
        }
        else if (Event == EInputEvent::Released)
        {
            bLeftMouseDragging = false;

            if (Gizmo != nullptr)
                Gizmo->OnMouseUp();
        }
        return true;
    }
    else if (Key == EKeys::RightMouseButton)
    {
        bRightMouseDragging = (Event == EInputEvent::Pressed);
        return true;
    }
    if (Event == EInputEvent::Pressed || Event == EInputEvent::Released)
    {
        if (Event == EInputEvent::Pressed && Key == EKeys::Space)
        {
            if (Gizmo != nullptr)
                Gizmo->ToggleMode();
        }
    }

    return false;
}

void FEditorViewportClient::MouseMove(FViewport *Viewport, int32 X, int32 Y)
{
    const int32_t DX = X - LastMouseX;
    const int32_t DY = Y - LastMouseY;
    LastMouseX = X;
    LastMouseY = Y;

    FRay ray = GetPickingRay();

    // 1. 좌클릭 드래그 중일 경우 기즈모 로직 수행
    if (bLeftMouseDragging && Gizmo != nullptr && Gizmo->GetTargetObject() != nullptr)
    {
        FRay ray = GetPickingRay();
        Gizmo->OnMouseMove(ray.Origin, ray.Direction);
        return;
    }

    // [추가] 2. 드래그 중이 아닐 때 기즈모 Hover 처리
    if (!bLeftMouseDragging && !bRightMouseDragging && Gizmo != nullptr && Gizmo->GetTargetObject() != nullptr)
    {
        FRay ray = GetPickingRay();
        Gizmo->OnMouseHover(ray.Origin, ray.Direction);
    }

    // 우클릭 드래그 중일 경우 카메라 회전 로직 수행
    if (!bRightMouseDragging)
        return;

    // Yaw += DX * RotSpeed,  Pitch += DY * RotSpeed
    FVector<float> Rot = CameraTransform.GetRotation();
    Rot.Y += static_cast<float>(DX) * RotSpeed; // Yaw
    Rot.X -= static_cast<float>(DY) * RotSpeed; // Pitch

    // Pitch 클램프 (-89 ~ 89)
    if (Rot.X > 89.f)
        Rot.X = 89.f;
    if (Rot.X < -89.f)
        Rot.X = -89.f;

    CameraTransform.SetRotation(Rot);
}

void FEditorViewportClient::InputAxis(FViewport *Viewport, FKey Key, float Delta, float DeltaTime)
{
    if (UImGuiManager::Get().IsCaptureMouse())
        return;

    const float PitchRad = CameraTransform.GetRotation().X * (3.14159265f / 180.f);
    const float YawRad = CameraTransform.GetRotation().Y * (3.14159265f / 180.f);

    // 카메라 로컬 Forward
    FVector4<float> Forward = {std::cos(PitchRad) * std::cos(YawRad), std::cos(PitchRad) * std::sin(YawRad), std::sin(PitchRad), 0.f};

    FVector4<float> Loc = CameraTransform.GetLocation();
    Loc += Forward * Delta * ZoomSpeed * DeltaTime;

    CameraTransform.SetLocation({Loc.X, Loc.Y, Loc.Z});
}

FMatrix<float> FEditorViewportClient::GetViewMatrix() const { return CameraTransform.ComputeOrbitMatrix(); }

FMatrix<float> FEditorViewportClient::GetProjectionMatrix(float width, float height)
{
    // 시야각(FOV) 계산
    float HalfFOV = CameraTransform.GetFOV() / 2.0f;

    // 뷰포트 종횡비(Aspect Ratio) 계산
    float AspectRatio = width / (height > 0.0f ? height : 1.0f);

    if (UImGuiManager::Get().bIsOrthogonal)
    {
        // 직교 투영
        FVector dir = CameraTransform.GetLocation() - CameraTransform.GetLookAt();
        float   Distance = dir.Length();
        float   OrthoWidth = Distance * 2.0f;
        float   OrthoHeight = OrthoWidth / AspectRatio;

        return FOrthographicMatrix<float>(OrthoWidth, OrthoHeight, 0.1f, 1000.0f);
    }

    // 원근 투영
    return FPerspectiveMatrix<float>(HalfFOV,            // HalfFOVX
                                     HalfFOV,            // HalfFOVY
                                     1.0f / AspectRatio, // MultFOVX
                                     1.0f,               // MultFOVY (화면 비율에 맞게 X, Y 비율 조정)
                                     0.1f,               // MinZ (Near Plane)
                                     1000.0f             // MaxZ (Far Plane)
    );
}

void FEditorViewportClient::Render(URenderer &renderer)
{
    FSceneViewOptions OriginalViewOptions;
    OriginalViewOptions.ViewMode = GetViewMode();
    OriginalViewOptions.bDrawAABB = GetDrawAABB();

    renderer.SetViewMode(EViewModeIndex::VMI_Lit);
    renderer.SetDrawAABB(false);

    if (UImGuiManager::Get().GetSelectedObject() == nullptr)
    {
        if (Gizmo != nullptr)
        {
            Gizmo->SetTargetObject(nullptr);
        }
    }

    renderer.SetViewMode(OriginalViewOptions.ViewMode);
    renderer.SetDrawAABB(OriginalViewOptions.bDrawAABB);
}

void FEditorViewportClient::ApplyMovement(float DeltaTime, FViewport *Viewport)
{
    if (UImGuiManager::Get().IsCaptureKeyboard())
        return;

    const float PitchRad = CameraTransform.GetRotation().X * (3.14159265f / 180.f);
    const float YawRad = CameraTransform.GetRotation().Y * (3.14159265f / 180.f);

    // 카메라 로컬 Forward / Right
    FVector4<float> Forward = {std::cos(PitchRad) * std::cos(YawRad), std::cos(PitchRad) * std::sin(YawRad), std::sin(PitchRad), 0.f};
    FVector4<float> Right = {std::cos(YawRad + 1.5707963f), std::sin(YawRad + 1.5707963f), 0.f, 0.f};
    FVector4<float> Up = Forward ^ Right;

    FVector4<float> Move = {0.f, 0.f, 0.f, 0.f};
    bool            bMoved = false;

    if (Viewport->KeyState(EKeys::W))
    {
        Move += Forward;
        bMoved = true;
    }
    if (Viewport->KeyState(EKeys::S))
    {
        Move -= Forward;
        bMoved = true;
    }
    if (Viewport->KeyState(EKeys::D))
    {
        Move += Right;
        bMoved = true;
    }
    if (Viewport->KeyState(EKeys::A))
    {
        Move -= Right;
        bMoved = true;
    }
    if (Viewport->KeyState(EKeys::Q))
    {
        Move += Up;
        bMoved = true;
    }
    if (Viewport->KeyState(EKeys::E))
    {
        Move -= Up;
        bMoved = true;
    }

    if (!bMoved)
        return;

    // 대각선 이동 시 속도가 빨라지지 않도록 함
    Move.Normalize();
    FVector4<float> deltaMove = Move * MoveSpeed * DeltaTime;

    FVector4<float> Loc(CameraTransform.GetLocation());
    Loc += deltaMove;
    CameraTransform.SetLocation({Loc.X, Loc.Y, Loc.Z});

    // char Buf[256];
    // snprintf(Buf, sizeof(Buf), "[Transform] Location: X=%.3f Y=%.3f Z=%.3f",
    //     CameraTransform.GetLocation().X, CameraTransform.GetLocation().Y, CameraTransform.GetLocation().Z,
    //     Forward.X, Forward.Y, Forward.Z
    //);
    // UImGuiManager::Get().AddLog(Buf);
}

FRay FEditorViewportClient::GetPickingRay()
{
    float MouseX = Viewport->GetMouseX();
    float MouseY = Viewport->GetMouseY();

    float ViewportWidth = Viewport->GetWidth();
    float ViewportHeight = Viewport->GetHeight();

    // 1. NDC
    float NDC_X = (2.0f * MouseX / ViewportWidth) - 1.0f;
    float NDC_Y = 1.0f - (2.0f * MouseY / ViewportHeight);

    FVector4<float> NDCNear = FVector4(NDC_X, NDC_Y, 0.0f, 1.0f);
    FVector4<float> NDCFar = FVector4(NDC_X, NDC_Y, 1.0f, 1.0f);

    // 2. Inverse ViewProjection Matrix
    FMatrix<float> ProjectionMatrix = GetProjectionMatrix(ViewportWidth, ViewportHeight);
    FMatrix<float> ViewMatrix = GetViewMatrix();

    FMatrix<float> ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
    FMatrix<float> InvViewProjection = ViewProjectionMatrix.Inverse();

    FVector4<float> WorldNear = NDCNear * InvViewProjection;
    FVector4<float> WorldFar = NDCFar * InvViewProjection;

    WorldNear /= WorldNear.W;
    WorldFar /= WorldFar.W;

    // 3. Ray 생성
    FVector<float> RayOrigin = FVector(WorldNear.X, WorldNear.Y, WorldNear.Z);
    FVector<float> RayDirection = FVector(WorldFar.X, WorldFar.Y, WorldFar.Z) - RayOrigin;
    RayDirection.Normalize();

    return FRay(RayOrigin, RayDirection);
}

void FEditorViewportClient::PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection) const
{
    // Show Primitives가 꺼져있다면 피킹을 수행하지 않고 선택을 해제합니다.
    if ((ShowFlags & EEngineShowFlags::SF_Primitives) == EEngineShowFlags::None)
    {
        if (Gizmo != nullptr)
            Gizmo->SetTargetObject(nullptr);
        UImGuiManager::Get().SetSelectedObject(nullptr);
        return;
    }

    FHitResult           ClosestHit = GWorld->PickingRay(RayOrigin, RayDirection);
    UPrimitiveComponent *HitComp = ClosestHit.HitComponent;

    if (ClosestHit.bHit && HitComp != nullptr)
    {
        // 1. 부딪힌 컴포넌트의 소유자(Actor)를 가져옵니다.
        TArray properties = HitComp->StaticClass()->GetProperties();
        // std::cout << properties.size() << std::endl;

        for (const auto &property : properties)
        {
            std::cout << property.Name << std::endl;
        }

        if (AActor *HitActor = Cast<AActor>(HitComp->GetOwner()))
        {
            // 2. 액터의 RootComponent를 기즈모의 타겟으로 설정합니다.
            if (USceneComponent *RootComp = HitActor->GetRootComponent())
            {
                if (Gizmo != nullptr)
                {
                    // 기즈모가 개별 메쉬가 아닌 액터 전체(Root)를 조작하게 됩니다.
                    Gizmo->SetTargetObject(RootComp);
                }

                // 충돌한 컴포넌트를 ImGui의 객체로 등록한다.
                UImGuiManager::Get().SetSelectedObject(HitComp);
                return;
            }
        }
    }
    else
    {
        if (Gizmo != nullptr)
            Gizmo->SetTargetObject(nullptr);
        UImGuiManager::Get().SetSelectedObject(nullptr);
    }
}

void FEditorViewportClient::LoadConfig()
{
    char   buffer[256];
    LPCSTR iniPath = ".\\editor.ini";

    // 1. MoveSpeed 로드 및 예외 처리 (범위를 벗어난 값이거나 이상한 값일 시 기본값 사용)
    GetPrivateProfileStringA("CameraSettings", "MoveSpeed", "5.0", buffer, sizeof(buffer), iniPath);
    try
    {
        float ParsedMoveSpeed = std::stof(buffer);
        if (ParsedMoveSpeed >= 0.1f && ParsedMoveSpeed <= 100.0f)
            MoveSpeed = ParsedMoveSpeed;
        else
            MoveSpeed = 5.0f;
    }
    catch (const std::exception &)
    {
        MoveSpeed = 5.0f;
    }

    // 2. RotSpeed 로드 및 예외 처리 (범위를 벗어난 값이거나 이상한 값일 시 기본값 사용)
    GetPrivateProfileStringA("CameraSettings", "RotSpeed", "0.1", buffer, sizeof(buffer), iniPath);
    try
    {
        float ParsedRotSpeed = std::stof(buffer);
        // ImGui 슬라이더 범위(0.01 ~ 0.5) 내의 정상적인 값인지 확인
        if (ParsedRotSpeed >= 0.01f && ParsedRotSpeed <= 0.5f)
            RotSpeed = ParsedRotSpeed;
        else
            RotSpeed = 0.1f;
    }
    catch (const std::exception &)
    {
        RotSpeed = 0.1f;
    }
}

void FEditorViewportClient::SaveConfig()
{
    LPCSTR iniPath = ".\\editor.ini";

    // float 값을 string으로 변환 후 ini 파일에 기록
    std::string moveStr = std::to_string(MoveSpeed);
    WritePrivateProfileStringA("CameraSettings", "MoveSpeed", moveStr.c_str(), iniPath);

    std::string rotStr = std::to_string(RotSpeed);
    WritePrivateProfileStringA("CameraSettings", "RotSpeed", rotStr.c_str(), iniPath);

    std::string gridStepStr = std::to_string(Grid->GetGridStep());
    WritePrivateProfileStringA("GridSettings", "GridStep", gridStepStr.c_str(), iniPath);
}

bool FInputEventState::IsLeftMouseButtonPressed() const { return IsButtonPressed(EKeys::LeftMouseButton); }

bool FInputEventState::IsRightMouseButtonPressed() const { return IsButtonPressed(EKeys::RightMouseButton); }

bool FInputEventState::IsButtonPressed(FKey InKey) const { return Viewport->KeyState(InKey); }
