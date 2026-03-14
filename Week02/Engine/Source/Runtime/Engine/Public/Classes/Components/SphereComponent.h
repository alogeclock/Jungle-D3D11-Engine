#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class USphereComponent : public UPrimitiveComponent
{
public:
	USphereComponent();
	virtual ~USphereComponent() override;

protected:
	float SphereRadius = 1.0f;
};