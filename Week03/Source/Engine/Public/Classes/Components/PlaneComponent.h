#pragma once
#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UPlaneComponent : public UPrimitiveComponent
{
  public:
    UPlaneComponent(const FString &InString);
    virtual ~UPlaneComponent() override;

    static UObject *Constructor() { return new UPlaneComponent("PlaneComponentConstructor"); }

    static UClass *StaticClass()
    {
        // 부모를 UPrimitiveComponent::StaticClass() 로 지정
        static UClass s_Class("UPlaneComponent", UPrimitiveComponent::StaticClass(), &UPlaneComponent::Constructor);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

  protected:
};