#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/EditorViewportClient.h"
#include "Engine/Source/Runtime/Core/Public/Math/ViewMatrix.h"

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
    Gizmo = new APivotTransformGizmo();
}

FEditorViewportClient::~FEditorViewportClient()
{
    if (Gizmo != nullptr)
    {
        delete Gizmo;
        Gizmo = nullptr;
    }
}

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
            bLeftMouseDragging = true;
            FRay ray = GetPickingRay();

            // 기즈모의 축(화살표)과 마우스 Ray가 충돌했는지 검사한 뒤, 기즈모를 클릭했다면 뒤의 씬 객체 피킹은 생략한다.
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

    // 좌클릭 드래그 중일 경우 기즈모 로직 수행
    if (bLeftMouseDragging && Gizmo != nullptr && Gizmo->GetTargetObject() != nullptr)
    {
        FRay ray = GetPickingRay();
        Gizmo->OnMouseMove(ray.Origin, ray.Direction);
        return;
    }

    // 우클릭 드래그 중일 경우 카메라 회전 로직 수행
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
    // --- 투영 행렬 생성 및 적용 부분 추가 ---
    // 시야각(FOV)을 90도로 설정 (절반인 45도를 라디안으로 변환)
    float HalfFOV = 3.14159265f / 4.0f;

    // 뷰포트 종횡비(Aspect Ratio) 계산
    float AspectRatio = width / (height > 0.0f ? height : 1.0f);

    if (UImGuiManager::Get().bIsOrthographic)
    {
        // 직교 투영
        FVector dir = CameraTransform.GetLocation() - CameraTransform.GetLookAt();
        float   Distance = dir.Length();
        float   OrthoWidth = Distance * FMath::Tan(HalfFOV) * 2.0f;
        float   OrthoHeight = Distance * FMath::Tan(HalfFOV) * 2.0f;
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

void FEditorViewportClient::RenderGizmo(URenderer& renderer)
{
    // 타겟 오브젝트가 설정되어 있을 때만 기즈모를 그린다.
    if (Gizmo != nullptr && Gizmo->GetTargetObject() != nullptr)
    {
        FMatrix<float> ViewMatrix = GetViewMatrix();
        Gizmo->Render(renderer, ViewMatrix);
    }
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
    float           NDC_X = (2.0f * MouseX / ViewportWidth) - 1.0f;
    float           NDC_Y = 1.0f - (2.0f * MouseY / ViewportHeight);

    FVector4<float> NDCNear = FVector4(NDC_X, NDC_Y, 0.0f, 1.0f);
    FVector4<float> NDCFar = FVector4(NDC_X, NDC_Y, 1.0f, 1.0f);

    // 2. Inverse ViewProjection Matrix
    FMatrix<float> ProjectionMatrix = GetProjectionMatrix(ViewportWidth, ViewportHeight);
    FMatrix<float> ViewMatrix = GetViewMatrix();

    FMatrix<float>  ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
    FMatrix<float>  InvViewProjection = ViewProjectionMatrix.Inverse();

    FVector4<float> WorldNear = NDCNear * InvViewProjection;
    FVector4<float> WorldFar = NDCFar * InvViewProjection;

    WorldNear /= WorldNear.W;
    WorldFar /= WorldFar.W;

    // 3. Ray 생성
    FVector<float> RayOrigin = FVector(WorldNear.X, WorldNear.Y, WorldNear.Z);
    FVector<float> RayDirection = FVector(WorldFar.X, WorldFar.Y, WorldFar.Z) - RayOrigin;
    RayDirection.Normalize();

    // Debug -------------
    char Buf[256];
    snprintf(Buf, sizeof(Buf), "Mouse Screen Position: X=%.3f Y=%.3f", MouseX, MouseY);
    UImGuiManager::Get().AddLog(Buf);

    snprintf(Buf, sizeof(Buf), "Mouse Screen Position(NDC): X=%.3f Y=%.3f", NDC_X, NDC_Y);
    UImGuiManager::Get().AddLog(Buf);

    snprintf(Buf, sizeof(Buf), "Mouse Ray Direction: X=%.3f Y=%.3f Z=%.3f\n\n", RayDirection.X, RayDirection.Y, RayDirection.Z);
    UImGuiManager::Get().AddLog(Buf);
    // -------------------

    return FRay(RayOrigin, RayDirection);
}

/// <summary>
/// 현재 씬에 생성된 오브젝트 모두 순회하면서 Ray Picking
/// -> 추후 씬 오브젝트 데이터 생기면 다른 파일(World.cpp)로 이동
/// </summary>
/// <param name="RayOrigin"></param>
/// <param name="RayDirection"></param>
/// <returns></returns>
FHitResult FEditorViewportClient::PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection)
{
    FHitResult ClosestHit;

    UPrimitiveComponent *Object = UImGuiManager::Get().GetSelectedObject(); // 피킹 된 거 Set으로 나중에 순서 변경

    if (Object == nullptr)
    {
        if (Gizmo != nullptr)
            Gizmo->SetTargetObject(nullptr);
        return ClosestHit;
    }

    // CurrentLevel->Actors 배열 순회
    //for (auto actor : CurrentLevel->Actors)
    //{
    //    FHitResult Hit = actor->IntersectRay(RayOrigin, RayDirection);

    //    if (Hit.bHit && Hit.Distance < ClosestHit.Distance)
    //    ClosestHit = Hit;
    //}

    FHitResult Hit = Object->IntersectRay(RayOrigin, RayDirection);

    char Buf[256];
    snprintf(Buf, sizeof(Buf), "Ray Distance: %.3f %s", Hit.Distance, Hit.bHit ? "HIT" : "Hit X");
    UImGuiManager::Get().AddLog(Buf);

    // 확인용
    if (Hit.bHit)
    {
        Object->Selected();
        if (Gizmo != nullptr)
            Gizmo->SetTargetObject(Object);
    }
    else
    {
        Object->NotSelected();
        if (Gizmo != nullptr)
            Gizmo->SetTargetObject(nullptr);
    }

    ClosestHit = Hit;

    return ClosestHit;
}

bool FInputEventState::IsLeftMouseButtonPressed() const { return IsButtonPressed(EKeys::LeftMouseButton); }

bool FInputEventState::IsRightMouseButtonPressed() const { return IsButtonPressed(EKeys::RightMouseButton); }

bool FInputEventState::IsButtonPressed(FKey InKey) const { return Viewport->KeyState(InKey); }
