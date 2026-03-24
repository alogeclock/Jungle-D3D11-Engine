#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UTriangleComponent : public UPrimitiveComponent
{
    DECLARE_OBJECT(UTriangleComponent, UPrimitiveComponent)

  public:
    UTriangleComponent(const FString &InString);
    virtual ~UTriangleComponent() override;

    virtual void Submit() override;

  protected:
};