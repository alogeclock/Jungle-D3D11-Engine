#include "Source/Engine/Public/Classes/Components/BillboardComponent.h"
#include "Source/Core/Public/Math/Vector4.h"
#include "Source/Engine/Public/Classes/TextureManager.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "World.h"
#include <cmath>

UBillboardComponent::UBillboardComponent(const FString& InString) : UPrimitiveComponent(InString)
{
    PrimitiveType = EPrimitiveType::Billboard;
    GpuBuffers = new FTextGpuBuffer();

    BillboardVertices = {
        { {0.5f, -0.5f, 0.f}, 1.0f, 1.0f},
        { {-0.5f, 0.5f, 0.f}, 0.0f, 0.0f},
        {  {0.5f, 0.5f, 0.f}, 1.0f, 0.0f},
        { {0.5f, -0.5f, 0.f}, 1.0f, 1.0f},
        {{-0.5f, -0.5f, 0.f}, 0.0f, 1.0f},
        { {-0.5f, 0.5f, 0.f}, 0.0f, 0.0f},
    };

    BillboardIndices = {0, 1, 2, 3, 4, 5};
}

UBillboardComponent::~UBillboardComponent()
{
    delete GpuBuffers;
    GpuBuffers = nullptr;
}

void UBillboardComponent::ApplyBillboardTransform(const FTransform& TargetTransform,
                                                  const FVector<float>& CameraRotation,
                                                  const FMatrix<float>& ViewMatrix, float FOV, bool bIsOrthogonal,
                                                  const FVector<float>& CameraLocation,
                                                  const FVector<float>& CameraLookAt)
{
    FTransform BillboardTransform;
    BillboardTransform.Location = TargetTransform.Location;
    BillboardTransform.Rotation = TargetTransform.Rotation;

    const float ScaleFactor =
        CalculateScaleFactor(TargetTransform, ViewMatrix, FOV, bIsOrthogonal, CameraLocation, CameraLookAt);
    BillboardTransform.Scale = FVector<float>(ScaleFactor, ScaleFactor, ScaleFactor);

    SetTransform(BillboardTransform);
}

float UBillboardComponent::CalculateScaleFactor(const FTransform& TargetTransform, const FMatrix<float>& ViewMatrix,
                                                float FOV, bool bIsOrthogonal, const FVector<float>& CameraLocation,
                                                const FVector<float>& CameraLookAt) const
{
    const float PI = 3.141592f;
    float OrthoWidth = 10.0f;
    if (bIsOrthogonal)
    {
        FVector<float> Direction = CameraLocation - CameraLookAt;
        OrthoWidth = Direction.Length() * 2.0f;
    }

    FVector4<float> TargetWorldPos(TargetTransform.Location.X, TargetTransform.Location.Y, TargetTransform.Location.Z,
                                   1.0f);
    FVector4<float> TargetViewPos = TargetWorldPos * ViewMatrix;

    float ScaleFactor = 1.0f;

    if (!bIsOrthogonal)
    {
        float ZDepth = std::abs(TargetViewPos.Z);

        float EuclideanDist = std::sqrt(TargetViewPos.X * TargetViewPos.X + TargetViewPos.Y * TargetViewPos.Y +
                                        TargetViewPos.Z * TargetViewPos.Z);

        float CosineAngle = ZDepth / EuclideanDist;
        float CorrectedDistance = ZDepth * CosineAngle;

        const float DistanceThreshold = 5.0f;
        float ClampedDistance = (std::min)(CorrectedDistance, DistanceThreshold);

        float FOVRad = FOV * (PI / 180.0f);
        float FOVScale = std::tan(FOVRad / 2.0f);

        ScaleFactor *= ClampedDistance * FOVScale * 20.0f;
    }
    else
    {
        ScaleFactor = OrthoWidth * 0.2f;
    }

    return ScaleFactor;
}

void UBillboardComponent::Render(URenderer& renderer)
{
    if (!IsRenderable(renderer))
        return;

    FMatrix<float> BillboardWorldMatrix = GetWorldMatrix();

    FVector<float> CameraRight;
    FVector<float> CameraUp;
    FVector<float> CameraForward;
    renderer.GetCameraBasis(CameraRight, CameraUp, CameraForward);

    FTransform WorldTransform;
    WorldTransform = WorldTransform.ToTransform(GetWorldMatrix());

    BillboardWorldMatrix = FMatrix<float>::Identity();

    BillboardWorldMatrix.M[0][0] = CameraRight.X * WorldTransform.Scale.X;
    BillboardWorldMatrix.M[0][1] = CameraRight.Y * WorldTransform.Scale.X;
    BillboardWorldMatrix.M[0][2] = CameraRight.Z * WorldTransform.Scale.X;

    BillboardWorldMatrix.M[1][0] = CameraUp.X * WorldTransform.Scale.Y;
    BillboardWorldMatrix.M[1][1] = CameraUp.Y * WorldTransform.Scale.Y;
    BillboardWorldMatrix.M[1][2] = CameraUp.Z * WorldTransform.Scale.Y;

    const FVector<float> FacingToCamera = CameraForward * -1.0f;
    BillboardWorldMatrix.M[2][0] = FacingToCamera.X * WorldTransform.Scale.Z;
    BillboardWorldMatrix.M[2][1] = FacingToCamera.Y * WorldTransform.Scale.Z;
    BillboardWorldMatrix.M[2][2] = FacingToCamera.Z * WorldTransform.Scale.Z;

    BillboardWorldMatrix.M[3][0] = WorldTransform.Location.X;
    BillboardWorldMatrix.M[3][1] = WorldTransform.Location.Y;
    BillboardWorldMatrix.M[3][2] = WorldTransform.Location.Z;
    BillboardWorldMatrix.M[3][3] = 1.0f;

    FConstants Constants = {};
    Constants.MVPMatrix = BillboardWorldMatrix;

    renderer.SetDepthStencilEnable(bEnableDepthTest);
    renderer.SetCullMode(CullMode);
    if (GpuBuffers)
    {
        renderer.RenderTextBatch(TexturePath, Constants, BillboardVertices, BillboardIndices, *GpuBuffers);
    }
    renderer.UndoRenderText();
}

void UBillboardComponent::Submit(const FSceneViewOptions& ViewOptions)
{
    UPrimitiveComponent::Submit(ViewOptions);
    FRenderCommand& Command = RenderProxy->RenderCommand;

    Command.bIsTextured = true;
    if (Command.VertexBuffer == nullptr)
    {
        Command.VertexBuffer = GpuBuffers->VertexBuffer.Get();
        Command.IndexBuffer = GpuBuffers->IndexBuffer.Get();
        Command.NumVertices = BillboardVertices.size();
        Command.NumIndices = BillboardIndices.size();
        Command.Stride = sizeof(FTextureVertex);
    }
}