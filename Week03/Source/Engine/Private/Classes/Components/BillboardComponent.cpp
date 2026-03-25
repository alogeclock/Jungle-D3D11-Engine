#include "Source/Engine/Public/Classes/Components/BillboardComponent.h"
#include "Source/Engine/Public/Classes/TextureManager.h"
#include "Source/Editor/Public/Application.h"
#include "Source/Editor/Public/EditorViewportClient.h"
#include "Source/Engine/Public/Classes/MeshManager.h"
#include "Source/Engine/Public/Classes/TextureManager.h"
#include "Source/Engine/Public/GUI/ImGuiManager.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "World.h"
#include <cmath>

namespace
{
FVector<float> NormalizeOr(const FVector<float>& InVector, const FVector<float>& Fallback)
{
    FVector<float> Result = InVector;
    if (!Result.Normalize())
    {
        return Fallback;
    }
    return Result;
}

float RowLength3(const FMatrix<float>& Matrix, int Row)
{
    return std::sqrt(Matrix.M[Row][0] * Matrix.M[Row][0] + Matrix.M[Row][1] * Matrix.M[Row][1] +
                     Matrix.M[Row][2] * Matrix.M[Row][2]);
}
} // namespace

void UBillboardComponent::Submit(const FSceneViewOptions& ViewOptions)
{
    UPrimitiveComponent::Submit(ViewOptions);

    if (!RenderProxy->RenderCommand.bIsVisible)
    {
        return;
    }

    FConstants Constants = {};
    Constants.MVPMatrix = BuildBillboardWorldMatrix();

    FRenderCommand& Command = RenderProxy->RenderCommand;

    Command.bIsTextured = bIsTextured;
    Command.CullMode = ECullMode::None;
    Command.TextureSRV = UTextureManager::Get().GetTexture(TexturePath);
    Command.Constants = Constants;

    if (Command.VertexBuffer == nullptr)
    {
        Command.VertexBuffer = UMeshManager::Get().GetVertexBuffer(PrimitiveType);
        Command.NumVertices = UMeshManager::Get().GetNumVertices(PrimitiveType);
        Command.Stride = sizeof(FTextureVertex);
    }
}

void UBillboardComponent::Render(URenderer &renderer)
{
    if (!IsRenderable(renderer))
        return;

    FConstants constants;
    constants.MVPMatrix = BuildBillboardWorldMatrix(); // 부모 및 자신의 변경 사항을 반영한 GetWorldMatrix()를 호출한다.

    renderer.SetDepthStencilEnable(bEnableDepthTest);
    renderer.SetCullMode(CullMode);
    renderer.SetTopology(this->Topology);

    FConstantsColor constantsColor(Color.X, Color.Y, Color.Z, Color.W);
    
    renderer.DeviceContext->VSSetShader(renderer.TextVertexShader, nullptr, 0);
    renderer.DeviceContext->PSSetShader(renderer.TextPixelShader, nullptr, 0);
    renderer.DeviceContext->IASetInputLayout(renderer.TextInputLayout);
    
    ID3D11ShaderResourceView* TextureSRV = UTextureManager::Get().GetTexture(TexturePath);

    renderer.DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);
    renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.LinearSamplerState);

    renderer.RenderPrimitive(this, constants, constantsColor);
}

void UBillboardComponent::ApplyBillboardTransform(const FTransform& TargetTransform, FViewportCameraTransform& Camera)
{
    FVector<float> CameraRight = CachedCameraRight;
    FVector<float> CameraUp = CachedCameraUp;
    FVector<float> CameraForward = CachedCameraForward;

    FMatrix<float> ViewMatrix = GEditor->GetEditorViewportClient()->GetViewMatrix();
    CameraRight = NormalizeOr(FVector<float>(ViewMatrix.M[0][0], ViewMatrix.M[1][0], ViewMatrix.M[2][0]),
                              FVector<float>(1.0f, 0.0f, 0.0f));
    CameraUp = NormalizeOr(FVector<float>(ViewMatrix.M[0][1], ViewMatrix.M[1][1], ViewMatrix.M[2][1]),
                           FVector<float>(0.0f, 0.0f, 1.0f));
    CameraForward = NormalizeOr(FVector<float>(ViewMatrix.M[0][2], ViewMatrix.M[1][2], ViewMatrix.M[2][2]),
                                FVector<float>(0.0f, 1.0f, 0.0f));

    CachedCameraRight = CameraRight;
    CachedCameraUp = CameraUp;
    CachedCameraForward = CameraForward;

    constexpr float SizeFactor = 0.3f;
    constexpr float DistanceThreshold = 3.0f;

    float ScaleFactor = 1.0f;
    const bool bIsOrthogonal = UImGuiManager::Get().bIsOrthogonal;

    if (!bIsOrthogonal)
    {
        const FVector<float> ToTarget = TargetTransform.Location - Camera.GetLocation();
        const float CorrectedDistance = ToTarget.Length();

        const float ClampedDistance = (std::min)(CorrectedDistance, DistanceThreshold);

        const float FOVRad = Camera.GetFOV();
        const float FOVScale = std::tan(FOVRad / 2.0f);

        ScaleFactor = ClampedDistance * FOVScale * SizeFactor;
    }
    else
    {
        const FVector<float> Direction = Camera.GetLocation() - Camera.GetLookAt();
        const float OrthoWidth = Camera.GetOrthoZoom() * 2.0f;
        ScaleFactor = OrthoWidth * SizeFactor;
    }

    FTransform BillboardTransform;
    BillboardTransform.Location = TargetTransform.Location;
    BillboardTransform.Rotation = FVector<float>(0.0f, 0.0f, 0.0f);
    BillboardTransform.Scale = FVector<float>(ScaleFactor, ScaleFactor, ScaleFactor);

    SetTransform(BillboardTransform);
}

FMatrix<float> UBillboardComponent::BuildBillboardWorldMatrix()
{
    const FMatrix<float>& World = GetWorldMatrix();
    const FVector<float> WorldPos(World.M[3][0], World.M[3][1], World.M[3][2]);
    const FVector<float> WorldScale(RowLength3(World, 0), RowLength3(World, 1), RowLength3(World, 2));

    const FVector<float> FacingToCamera = CachedCameraForward * -1.0f;

    return FMatrix<float>(FPlane<float>(CachedCameraRight.X * WorldScale.X, CachedCameraRight.Y * WorldScale.X,
                                        CachedCameraRight.Z * WorldScale.X, 0.0f),
                          FPlane<float>(CachedCameraUp.X * WorldScale.Y, CachedCameraUp.Y * WorldScale.Y,
                                        CachedCameraUp.Z * WorldScale.Y, 0.0f),
                          FPlane<float>(FacingToCamera.X * WorldScale.Z, FacingToCamera.Y * WorldScale.Z,
                                        FacingToCamera.Z * WorldScale.Z, 0.0f),
                          FPlane<float>(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.0f));
}