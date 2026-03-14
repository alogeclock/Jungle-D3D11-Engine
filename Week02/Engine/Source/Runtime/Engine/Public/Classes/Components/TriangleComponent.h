#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UTriangleComponent : public UPrimitiveComponent
{
public:
	UTriangleComponent();
	virtual ~UTriangleComponent() override;

protected:
	float SphereRadius = 1.0f;
};