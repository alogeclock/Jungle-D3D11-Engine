#include "Source/Engine/Public/Classes/Components/ParticleSubUVComponent.h"
#include "Source/Engine/Public/Classes/TextMeshBuilder.h"
#include <CMath>
#include "CoreTypes.h"
#include <iostream>

UParticleSubUVComponent::UParticleSubUVComponent(const FString &InString) : UTextComponent(InString)
{ 
    FilePath = "Data/Texture/Explosion.png"; 
    SpriteSize = 150;
    Height = 900;
    Width = 900;
    bMeshDirty = true;
    CurrentTime = 0;
    bLoop = true;

    SetScale({3.f, 3.f, 3.f});
}

void UParticleSubUVComponent::Render(URenderer &renderer)
{
    if (CurrentTime >= PlayRate && !bLoop)
        return;
    UTextComponent::Render(renderer);
}

void UParticleSubUVComponent::Tick(float deltaTime)
{
    CurrentTime += deltaTime;
    if (bLoop && CurrentTime >= PlayRate)
        CurrentTime -= PlayRate;
}

void UParticleSubUVComponent::RebuildMesh()
{
    //std::cout << FilePath << std::endl;
    TextVertices.reserve(6);

    uint32 Row = Height / SpriteSize;
    uint32 Collum = Width / SpriteSize;

    uint32 Size = Row * Collum;
    //std::cout << Size << std::endl;

    float  CurrentState = CurrentTime / PlayRate;

    int CurrentIndex = static_cast<int>(floor(Size * CurrentState));

    uint32 u0, u1, v0, v1;
    u0 = CurrentIndex % Collum;
    v0 = CurrentIndex / Collum;

    u1 = u0 + 1;
    v1 = v0 + 1;

    u0 *= SpriteSize;
    u1 *= SpriteSize;
    v0 *= SpriteSize;
    v1 *= SpriteSize;

    float x0 = -0.5f;
    float x1 = 0.5f;
    float y0 = -0.5f;
    float y1 = 0.5f;

    float fu0 = (float)u0 / (float)Width;
    float fu1 = (float)u1 / (float)Width;
    float fv0 = (float)v0 / (float)Height;
    float fv1 = (float)v1 / (float)Height;

    TextVertices.clear();

    TextVertices.push_back({FVector<float>(x0, y0, 0.f), fu0, fv0});
    TextVertices.push_back({FVector<float>(x1, y0, 0.f), fu1, fv0});
    TextVertices.push_back({FVector<float>(x0, y1, 0.f), fu0, fv1});
    TextVertices.push_back({FVector<float>(x1, y1, 0.f), fu1, fv1});

    TextIndeices.push_back(0);
    TextIndeices.push_back(1);
    TextIndeices.push_back(2);
    TextIndeices.push_back(1);
    TextIndeices.push_back(3);
    TextIndeices.push_back(2);
    bMeshDirty = true;
}

void UParticleSubUVComponent::Submit()
{
    if (!RenderProxy) return;

    UPrimitiveComponent::Submit();
    FRenderCommand &Command = RenderProxy->RenderCommand;

    Command.VertexBuffer = UMeshManager::Get().GetVertexBuffer(PrimitiveType);
    Command.IndexBuffer = UMeshManager::Get().GetIndexBuffer(PrimitiveType);
    Command.NumVertices = UMeshManager::Get().GetNumVertices(PrimitiveType);
    Command.NumIndices = UMeshManager::Get().GetNumIndices(PrimitiveType);
    Command.Stride = sizeof(FVertex);
}