#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UArrowComponent : public UPrimitiveComponent
{
public:
    UArrowComponent(const FString &InString);
	virtual ~UArrowComponent() override;

		static UObject *Constructor() { return new UArrowComponent("ArrowComponentConstructor"); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("UArrowComponent", UPrimitiveComponent::StaticClass(), &UArrowComponent::Constructor);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
};