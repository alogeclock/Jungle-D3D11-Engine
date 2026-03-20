#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UCubeComponent : public UPrimitiveComponent
{
public:
    UCubeComponent(const FString &InString);
	virtual ~UCubeComponent() override;

	static UObject *Constructor() { return new UCubeComponent("CubeComponentConstructor"); }

    static UClass *StaticClass()
    {
        // 부모를 UPrimitiveComponent::StaticClass() 로 지정
        static UClass s_Class("UCubeComponent", UPrimitiveComponent::StaticClass(), &UCubeComponent::Constructor);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

protected:
};
