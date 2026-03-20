#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class USphereComponent : public UPrimitiveComponent
{
    DECLARE_OBJECT(USphereComponent, UPrimitiveComponent)
public:
    USphereComponent(const FString &InString, float inSphereRadius = 1.0f);
    virtual ~USphereComponent() override;

      protected:
	float SphereRadius = 1.0f;
};