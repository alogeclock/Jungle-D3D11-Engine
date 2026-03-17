#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UGridComponent : public UPrimitiveComponent
{
public:
	UGridComponent() ;
	virtual ~UGridComponent() override;

	static UObject *ConstructGridComponent() { return new UGridComponent(); }

    static UClass *StaticClass()
    {
        // 부모를 UPrimitiveComponent::StaticClass() 로 지정
        static UClass s_Class("UGridComponent", UPrimitiveComponent::StaticClass(), &UGridComponent::ConstructGridComponent);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

protected:
};
