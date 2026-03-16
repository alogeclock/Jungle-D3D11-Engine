#pragma once
#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UTorusComponent : public UPrimitiveComponent
{
public:
	UTorusComponent() ;
	virtual ~UTorusComponent() override;

		static UObject *ConstructCubeComponent() { return new UTorusComponent(); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("UTorusComponent", UPrimitiveComponent::StaticClass(), &UTorusComponent::ConstructCubeComponent);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
};