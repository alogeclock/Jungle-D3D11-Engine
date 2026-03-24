#pragma once

#include "Source/Engine/Public/Classes/Components/UUIDTextComponent.h"

class UParticleSubUVComponent : public UTextComponent
{
    DECLARE_OBJECT_START(UParticleSubUVComponent, UTextComponent)
    PUBLIC_PROPERTY(UParticleSubUVComponent, bLoop, Bool)
    PUBLIC_PROPERTY(UParticleSubUVComponent, PlayRate, Float)
    DECLARE_END 
public :
    UParticleSubUVComponent(const FString &InString);

    bool   bLoop;
    float  PlayRate = 1.f;
    float  CurrentTime;
    uint32 Width;
    uint32 Height;
    uint32 SpriteSize;

    virtual void Render(URenderer &renderer) override;
    virtual void Tick(float deltaTime) override;
    virtual void RebuildMesh() override;

    virtual void Submit() override;
    virtual FRenderProxy* CreateRenderProxy() override;
};

