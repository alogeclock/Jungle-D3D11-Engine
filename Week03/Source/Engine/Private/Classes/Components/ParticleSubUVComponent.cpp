#include "Source/Engine/Public/Classes/Components/ParticleSubUVComponent.h"
#include "Source/Engine/Public/Classes/TextureManager.h"
#include "Source/Engine/Public/Classes/TextMeshBuilder.h"
#include <CMath>
#include "CoreTypes.h"
#include <iostream>

UParticleSubUVComponent::UParticleSubUVComponent(const FString &InString) : UPrimitiveComponent(InString)
{ 
    FilePath = "Data/Texture/Explosion.png"; 
    SpriteSize = 150;
    Height = 900;
    Width = 900;
    CurrentTime = 0;
    bLoop = true;
    PrimitiveType = EPrimitiveType::SubUV;
    CullMode = ECullMode::None;
    TotalFrameCount = 36;

    SetScale({3.f, 3.f, 3.f});
}

void UParticleSubUVComponent::Render(URenderer &renderer)
{
}

void UParticleSubUVComponent::Tick(float deltaTime)
{
    PlaySpeed = max(PlaySpeed, 0);
    CurrentTime += deltaTime * TotalFrameCount * PlaySpeed;
    if (bLoop && CurrentTime >= TotalFrameCount)
        CurrentTime = 0;
    
    //RebuildMesh();
}

void UParticleSubUVComponent::Submit(const FSceneViewOptions& ViewOptions)
{
    if (!RenderProxy) return;

    UPrimitiveComponent::Submit(ViewOptions);
    FRenderCommand &Command = RenderProxy->RenderCommand;

    Command.bIsTextured = true;
    Command.CurrentFrame = static_cast<uint32>(CurrentTime);

    // 정적 Vertex Buffer의 경우 처음 한 번 외에는 호출하지 않음
    if (Command.VertexBuffer == nullptr)
    {
        Command.TextureSRV = UTextureManager::Get().GetTexture(FilePath);
        Command.VertexBuffer = UMeshManager::Get().GetVertexBuffer(PrimitiveType);
        Command.NumVertices = UMeshManager::Get().GetNumVertices(PrimitiveType);
        Command.Stride = sizeof(FTextureVertex);
    }
}

FHitResult UParticleSubUVComponent::IntersectRayMeshTriangle(
    const FVector<float>& RayOrigin,
    const FVector<float>& RayDirection)
{
    FHitResult Result;

    const FMatrix<float> WorldMatrix = GetWorldMatrix();

    for (uint32 Index = 0; Index + 2 < static_cast<uint32>(RayCheck.size()); Index += 3)
    {
        const FVector<float>& LocalV0 = RayCheck[Index + 0];
        const FVector<float>& LocalV1 = RayCheck[Index + 1];
        const FVector<float>& LocalV2 = RayCheck[Index + 2];

        const FVector4<float> V0L(LocalV0.X, LocalV0.Y, LocalV0.Z, 1.0f);
        const FVector4<float> V1L(LocalV1.X, LocalV1.Y, LocalV1.Z, 1.0f);
        const FVector4<float> V2L(LocalV2.X, LocalV2.Y, LocalV2.Z, 1.0f);

        const FVector4<float> V0W = V0L * WorldMatrix;
        const FVector4<float> V1W = V1L * WorldMatrix;
        const FVector4<float> V2W = V2L * WorldMatrix;

        const FVector<float> V0(V0W.X, V0W.Y, V0W.Z);
        const FVector<float> V1(V1W.X, V1W.Y, V1W.Z);
        const FVector<float> V2(V2W.X, V2W.Y, V2W.Z);

        float T = 0.0f;
        if (RayIntersectsTriangle(RayOrigin, RayDirection, V0, V1, V2, T) && T < Result.Distance)
        {
            Result.bHit = true;
            Result.Distance = T;
            Result.HitPoint = RayOrigin + RayDirection * T;
            Result.HitComponent = this;
        }
    }

    return Result;
}
