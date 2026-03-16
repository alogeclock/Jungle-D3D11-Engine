#pragma once
#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UPlaneComponent : public UPrimitiveComponent
{
public:
	UPlaneComponent() ;
	virtual ~UPlaneComponent() override;

		static UObject *ConstructCubeComponent() { return new UPlaneComponent(); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("UPlaneComponent", UPrimitiveComponent::StaticClass(), &UPlaneComponent::ConstructCubeComponent);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
};