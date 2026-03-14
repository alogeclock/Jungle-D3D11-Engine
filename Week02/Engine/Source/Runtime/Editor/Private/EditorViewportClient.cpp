#include "Engine/Source/Runtime/Editor/Public/EditorViewportClient.h"
#include "Engine/Source/Runtime/Core/Public/Math/ViewMatrix.h"

FViewportCameraTransform::FViewportCameraTransform()
	: ViewLocation(0.0f, 0.0f, 300.f)
	, ViewRotation(-30.f, 0.f, 0.f)    // Pitch -30
    , LookAt(0.f, 0.f, 0.f)
    , OrthoZoom(1.f), Max_OrthoZoom(1000.f), Min_OrthoZoom(1.f)
    , MaxLocation(1000.0)
{

}

void FViewportCameraTransform::SetLocation(const FVector<float>& Position)
{
    auto Clamp = [&](float V) -> float {
        return V < -(float)MaxLocation ? -(float)MaxLocation
            : V >(float)MaxLocation ? (float)MaxLocation
            : V;
        };
    ViewLocation = { Clamp(Position.X), Clamp(Position.Y), Clamp(Position.Z) };
}

FMatrix<float> FViewportCameraTransform::ComputeOrbitMatrix() const
{
    FVector<float> worldUp = { 0.0f, 0.0f, 1.0f };
    FMatrix<float> viewMatrix = FViewMatrix(ViewLocation, LookAt, worldUp);

    return viewMatrix;
}

void FEditorViewportClient::Tick(float DeltaTime, FViewport* Viewport)
{
    if (!Viewport) return;
    ApplyMovement(DeltaTime, Viewport);
}

bool FEditorViewportClient::InputKey(const FInputEventState& InputState)
{
    const EInputEvent Event = InputState.GetInputEvent();
    const FKey        Key = InputState.GetKey();

    // 우클릭 드래그 시작/종료
    if (Key == EKeys::RightMouseButton)
    {
        bRightMouseDragging = (Event == EInputEvent::Pressed);

        // 우클릭 이벤트




        return true;
    }

    // 키 누름 디버그 출력 (Pressed / Released만)
    if (Event == EInputEvent::Pressed || Event == EInputEvent::Released)
    {
        // 키보드 이벤트



    }

    return false;
}

void FEditorViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y) 
{
    const int32_t DX = X - LastMouseX;
    const int32_t DY = Y - LastMouseY;
    LastMouseX = X;
    LastMouseY = Y;

    if (!bRightMouseDragging) return;

    // Yaw += DX * RotSpeed,  Pitch += DY * RotSpeed
    FVector<float> Rot = CameraTransform.GetRotation();
    Rot.Y += static_cast<float>(DX) * RotSpeed;   // Yaw
    Rot.X += static_cast<float>(DY) * RotSpeed;   // Pitch

    // Pitch 클램프 (-89 ~ 89)
    if (Rot.X > 89.f) Rot.X = 89.f;
    if (Rot.X < -89.f) Rot.X = -89.f;

    CameraTransform.SetRotation(Rot);
}

void FEditorViewportClient::InputAxis(FViewport* Viewport, FKey Key, float Delta, float DeltaTime) 
{
    FVector<float> Loc = CameraTransform.GetLocation();
    Loc.Z -= Delta * ZoomSpeed * DeltaTime;
    CameraTransform.SetLocation(Loc);
}

FMatrix<float> FEditorViewportClient::GetViewMatrix() const
{
    return CameraTransform.ComputeOrbitMatrix();
}

void FEditorViewportClient::ApplyMovement(float DeltaTime, FViewport* Viewport)
{
    const float Rot_Y = CameraTransform.GetRotation().Y;
    const float YawRad = Rot_Y * (3.14159265f / 180.f);

    // 카메라 로컬 Forward / Right
    FVector4<float> Forward = { std::cos(YawRad), std::sin(YawRad), 0.f, 0.f };
    FVector4<float> Right = { std::cos(YawRad - 1.5707963f), std::sin(YawRad - 1.5707963f), 0.f, 0.f };

    FVector4<float> Move = { 0.f, 0.f, 0.f, 0.f };
    bool bMoved = false;

    if (Viewport->KeyState(EKeys::W)) { Move.X += Forward.X; Move.Y += Forward.Y; bMoved = true; }
    if (Viewport->KeyState(EKeys::S)) { Move.X -= Forward.X; Move.Y -= Forward.Y; bMoved = true; }
    if (Viewport->KeyState(EKeys::D)) { Move.X += Right.X;   Move.Y += Right.Y;   bMoved = true; }
    if (Viewport->KeyState(EKeys::A)) { Move.X -= Right.X;   Move.Y -= Right.Y;   bMoved = true; }


    if (!bMoved) return;

    FVector<float> Loc = CameraTransform.GetLocation();
    Loc.X += Move.X * MoveSpeed * DeltaTime;
    Loc.Y += Move.Y * MoveSpeed * DeltaTime;
    Loc.Z += Move.Z * MoveSpeed * DeltaTime;
    CameraTransform.SetLocation(Loc);

    char Buf[256];
    snprintf(Buf, sizeof(Buf),
        "[Transform] Location: X=%.3f Y=%.3f Z=%.3f",
        CameraTransform.GetLocation().X,
        CameraTransform.GetLocation().Y,
        CameraTransform.GetLocation().Z
    );
    UImGuiManager::Get().AddLog(Buf);
}

bool FInputEventState::IsLeftMouseButtonPressed() const
{
    return IsButtonPressed(EKeys::LeftMouseButton);
}

bool FInputEventState::IsRightMouseButtonPressed() const
{
    return IsButtonPressed(EKeys::RightMouseButton);
}

bool FInputEventState::IsButtonPressed(FKey InKey) const
{
    return Viewport->KeyState(InKey);
}
