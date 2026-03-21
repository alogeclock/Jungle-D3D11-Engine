#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UParticleSubUVComponent : public UPrimitiveComponent
{
    DECLARE_OBJECT(UParticleSubUVComponent, UPrimitiveComponent)
  public:
    UParticleSubUVComponent(const FString &InString);
    bool  bLoop;
    float PlayRate;

    virtual void Render(URenderer &renderer) override;



};