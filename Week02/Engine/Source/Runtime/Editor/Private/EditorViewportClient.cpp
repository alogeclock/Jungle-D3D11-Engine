#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/EditorViewportClient.h"
#include "Engine/Source/Runtime/Core/Public/Math/ViewMatrix.h"

FViewportCameraTransform::FViewportCameraTransform()
    : ViewLocation(-2.0f, 0.5f, 2.f), ViewRotation(-30.f, 0.f, 0.f),
      LookAt(0.f, 0.f, 0.f), OrthoZoom(1.f), Max_OrthoZoom(1000.f), Min_OrthoZoom(1.f), MaxLocation(1000.0)
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

    // 1. ViewRotation을 바탕으로 실제 카메라가 바라보는 Forward 벡터 계산
    const float PitchRad = ViewRotation.X * (3.14159265f / 180.f);
    const float YawRad = ViewRotation.Y * (3.14159265f / 180.f);

    FVector<float> Forward = {
        std::cos(PitchRad) * std::cos(YawRad),
        std::cos(PitchRad) * std::sin(YawRad),
        std::sin(PitchRad)
    };

    // 2. 카메라 위치(ViewLocation)에서 Forward 방향으로 뻗어나간 지점을 새로운 LookAt으로 설정
    FVector<float> DynamicLookAt = {
        ViewLocation.X + Forward.X,
        ViewLocation.Y + Forward.Y,
        ViewLocation.Z + Forward.Z
    };

    // 3. 동적으로 생성된 LookAt을 ViewMatrix에 적용
    return FViewMatrix(ViewLocation, DynamicLookAt, worldUp);
} 

FEditorViewportClient::FEditorViewportClient(FViewport *viewport) { Viewport = viewport; }

void FEditorViewportClient::Tick(float DeltaTime, FViewport *Viewport)
{
    if (!Viewport)
        return;
    ApplyMovement(DeltaTime, Viewport);
}

bool FEditorViewportClient::InputKey(const FInputEventState &InputState)
{
    const EInputEvent Event = InputState.GetInputEvent();
    const FKey        Key = InputState.GetKey();

    if (Key == EKeys::LeftMouseButton)
    {
        if (Event == EInputEvent::Pressed)
        {
            FRay ray = GetPickingRay();
            PickingRay(ray.Origin, ray.Direction);
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
        // 키보드 이벤트
    }

    return false;
}

void FEditorViewportClient::MouseMove(FViewport *Viewport, int32 X, int32 Y)
{
    const int32_t DX = X - LastMouseX;
    const int32_t DY = Y - LastMouseY;
    LastMouseX = X;
    LastMouseY = Y;

    if (!bRightMouseDragging)
        return;

    // Yaw += DX * RotSpeed,  Pitch += DY * RotSpeed
    FVector<float> Rot = CameraTransform.GetRotation();
    Rot.Y += static_cast<float>(DX) * RotSpeed; // Yaw
    Rot.X += static_cast<float>(DY) * RotSpeed; // Pitch

    // Pitch 클램프 (-89 ~ 89)
    if (Rot.X > 89.f)
        Rot.X = 89.f;
    if (Rot.X < -89.f)
        Rot.X = -89.f;

    CameraTransform.SetRotation(Rot);
}

void FEditorViewportClient::InputAxis(FViewport *Viewport, FKey Key, float Delta, float DeltaTime)
{
    FVector<float> Loc = CameraTransform.GetLocation();
    Loc.X += Delta * ZoomSpeed * DeltaTime;
    CameraTransform.SetLocation(Loc);
}

FMatrix<float> FEditorViewportClient::GetViewMatrix() const { return CameraTransform.ComputeOrbitMatrix(); }

FMatrix<float> FEditorViewportClient::GetProjectionMatrix(float width, float height)
{
    SetWidth(width);
    SetHeight(height);

    // --- 투영 행렬 생성 및 적용 부분 추가 ---
    // 시야각(FOV)을 90도로 설정 (절반인 45도를 라디안으로 변환)
    float HalfFOV = 3.14159265f / 4.0f;

    // 뷰포트 종횡비(Aspect Ratio) 계산
    float AspectRatio = width / (height > 0.0f ? height : 1.0f);

    // FPerspectiveMatrix(HalfFOVX, HalfFOVY, MultFOVX, MultFOVY, MinZ, MaxZ)
    return FPerspectiveMatrix<float>(
        HalfFOV,            // HalfFOVX
        HalfFOV,            // HalfFOVY
        1.0f / AspectRatio, // MultFOVX
        1.0f,               // MultFOVY (화면 비율에 맞게 X, Y 비율 조정)
        0.1f,               // MinZ (Near Plane)
        1000.0f             // MaxZ (Far Plane)
    );
}

void FEditorViewportClient::ApplyMovement(float DeltaTime, FViewport *Viewport)
{
    const float PitchRad = CameraTransform.GetRotation().X * (3.14159265f / 180.f);
    const float YawRad = CameraTransform.GetRotation().Y * (3.14159265f / 180.f);

    // 카메라 로컬 Forward / Right
    FVector4<float> Forward = {std::cos(PitchRad) * std::cos(YawRad), std::cos(PitchRad) * std::sin(YawRad), std::sin(PitchRad), 0.f};
    FVector4<float> Right = {std::cos(YawRad + 1.5707963f), std::sin(YawRad + 1.5707963f), 0.f, 0.f};
    FVector4<float> Up = Forward ^ Right;

    FVector4<float> Move = {0.f, 0.f, 0.f, 0.f};
    bool            bMoved = false;

    if (Viewport->KeyState(EKeys::W)) { Move += Forward; bMoved = true; }
    if (Viewport->KeyState(EKeys::S)) { Move -= Forward; bMoved = true; }
    if (Viewport->KeyState(EKeys::D)) { Move += Right;   bMoved = true; }
    if (Viewport->KeyState(EKeys::A)) { Move -= Right;   bMoved = true; }
    if (Viewport->KeyState(EKeys::Q)) { Move += Up;      bMoved = true; }
    if (Viewport->KeyState(EKeys::E)) { Move -= Up;      bMoved = true; }

    if (!bMoved)
        return;

    // 대각선 이동 시 속도가 빨라지지 않도록 함
    Move.Normalize();
    FVector4<float> deltaMove = Move * MoveSpeed * DeltaTime;

    FVector4<float> Loc(CameraTransform.GetLocation());
    Loc += deltaMove;
    CameraTransform.SetLocation({Loc.X, Loc.Y, Loc.Z});

    //char Buf[256];
    //snprintf(Buf, sizeof(Buf), "[Transform] Location: X=%.3f Y=%.3f Z=%.3f", 
    //    CameraTransform.GetLocation().X, CameraTransform.GetLocation().Y, CameraTransform.GetLocation().Z, 
    //    Forward.X, Forward.Y, Forward.Z
    //);
    //UImGuiManager::Get().AddLog(Buf);
}

FRay FEditorViewportClient::GetPickingRay()
{ 
    float MouseX = Viewport->GetMouseX();
    float MouseY = Viewport->GetMouseY();

    float ViewportWidth = Width;
    float ViewportHeight = Height;

    // 1. NDC
    float NDC_X = (2.0f * MouseX / ViewportWidth) - 1.0f;
    float NDC_Y = 1.0f - (2.0f * MouseY / ViewportHeight);
    FVector4<float> NDCPos = FVector4(NDC_X, NDC_Y, 1.0f, 1.0f);

    // 2. Projection
    auto projectionMatrix = GetProjectionMatrix(ViewportWidth, ViewportHeight);
    auto inverse_proj = projectionMatrix.Inverse();
    FVector4<float> ViewPos = NDCPos * inverse_proj;
    ViewPos /= ViewPos.W; // Perspective divide 역산

    // 3. View
    auto viewMatrix = GetViewMatrix();
    auto inverse_view = viewMatrix.Inverse();
    FVector4<float> WorldPos = ViewPos * inverse_view;

    // 4. Ray 생성
    FVector<float> RayOrigin = CameraTransform.GetLocation();
    FVector<float> RayDirection = FVector(WorldPos.X, WorldPos.Y, WorldPos.Z) - RayOrigin;
    RayDirection.Normalize();

    // Debug ------------- 
    char Buf[256];
    snprintf(Buf, sizeof(Buf), "Mouse Screen Position: X=%.3f Y=%.3f", MouseX, MouseY);
    UImGuiManager::Get().AddLog(Buf);

    snprintf(Buf, sizeof(Buf), "Mouse Screen Position(NDC): X=%.3f Y=%.3f", NDC_X, NDC_Y);
    UImGuiManager::Get().AddLog(Buf);

    snprintf(Buf, sizeof(Buf), "Mouse Ray Direction: X=%.3f Y=%.3f Z=%.3f\n\n", 
            RayDirection.X, RayDirection.Y, RayDirection.Z);
    UImGuiManager::Get().AddLog(Buf);
    // -------------------

    return FRay(RayOrigin, RayDirection);
}

/// <summary>
/// 현재 씬에 생성된 오브젝트 모두 순회하면서 Ray Picking
/// -> 추후 씬 오브젝트 데이터 생기면 다른 파일로 이동
/// </summary>
/// <param name="RayOrigin"></param>
/// <param name="RayDirection"></param>
/// <returns></returns>
FHitResult FEditorViewportClient::PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection)
{
    FHitResult ClosestHit;

    UPrimitiveComponent* Object = UImGuiManager::Get().GetSelectedObject(); // 피킹 된 거 Set으로 나중에 순서 변경
    FHitResult Hit = Object->IntersectRay(RayOrigin, RayDirection);

    char Buf[256];
    snprintf(Buf, sizeof(Buf), "Ray Distance: %.3f %s", Hit.Distance, Hit.bHit ? "HIT" : "Hit X");
    UImGuiManager::Get().AddLog(Buf);

    // 확인용
    if (Hit.bHit)
    {
        Object->Selected();
    }
    else
        Object->NotSelected();

    ClosestHit = Hit;
    
    return ClosestHit;
}

bool FInputEventState::IsLeftMouseButtonPressed() const { return IsButtonPressed(EKeys::LeftMouseButton); }

bool FInputEventState::IsRightMouseButtonPressed() const { return IsButtonPressed(EKeys::RightMouseButton); }

bool FInputEventState::IsButtonPressed(FKey InKey) const { return Viewport->KeyState(InKey); }
