#pragma once

#include "Source/Engine/Public/Classes/Components/UUIDTextComponent.h"

class UParticleSubUVComponent : public UPrimitiveComponent
{
    DECLARE_OBJECT_START(UParticleSubUVComponent, UPrimitiveComponent)
    PUBLIC_PROPERTY(UParticleSubUVComponent, bLoop, Bool)
    PUBLIC_PROPERTY(UParticleSubUVComponent, PlaySpeed, Float)
    DECLARE_END 
public :
    UParticleSubUVComponent(const FString &InString);

    bool   bLoop;
    float  PlaySpeed = 1.f;
    float  CurrentTime;
    uint32 Width;
    uint32 Height;
    uint32 SpriteSize;
    uint32 TotalFrameCount;

    FString FilePath;

    virtual void Render(URenderer &renderer) override;
    virtual void Tick(float deltaTime) override;
    //virtual void RebuildMesh();

    virtual void Submit(const FSceneViewOptions& ViewOptions) override;
    FHitResult IntersectRayMeshTriangle(const FVector<float> &RayOrigin, const FVector<float> &RayDirection) override;

    TArray<FVector<float>> RayCheck = {
        {0.5f, -0.5f, 0.f},
        {-0.5f, 0.5f, 0.f},
        {0.5f, 0.5f, 0.f},
        {0.5f, -0.5f, 0.f},
        {-0.5f, -0.5f, 0.f},
        {-0.5f, 0.5f, 0.f},
    };

};

