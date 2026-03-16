#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UCubeComponent : public UPrimitiveComponent
{
public:
	UCubeComponent() ;
	virtual ~UCubeComponent() override;

	static UObject *ConstructCubeComponent() { return new UCubeComponent(); }

    static UClass *StaticClass()
    {
        // 부모를 UPrimitiveComponent::StaticClass() 로 지정
        static UClass s_Class("UCubeComponent", UPrimitiveComponent::StaticClass(), &UCubeComponent::ConstructCubeComponent);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

protected:
};
