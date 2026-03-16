#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UArrowComponent : public UPrimitiveComponent
{
public:
	UArrowComponent() ;
	virtual ~UArrowComponent() override;

		static UObject *ConstructCubeComponent() { return new UArrowComponent(); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("UArrowComponent", UPrimitiveComponent::StaticClass(), &UArrowComponent::ConstructCubeComponent);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
};