#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UTriangleComponent : public UPrimitiveComponent
{
  public:
    UTriangleComponent(const FString &InString);
    virtual ~UTriangleComponent() override;

    static UObject *Constructor() { return new UTriangleComponent("TriangleComponentConstructor"); }

    static UClass *StaticClass()
    {
        // 부모를 UPrimitiveComponent::StaticClass() 로 지정
        static UClass s_Class("UTriangleComponent", UPrimitiveComponent::StaticClass(), &UTriangleComponent::Constructor);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

  protected:
};