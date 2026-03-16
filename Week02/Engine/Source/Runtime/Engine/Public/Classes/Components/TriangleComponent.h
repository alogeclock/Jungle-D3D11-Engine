#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UTriangleComponent : public UPrimitiveComponent
{
public:
	UTriangleComponent() ;
	virtual ~UTriangleComponent() override;

		static UObject *ConstructCubeComponent() { return new UTriangleComponent(); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("UTriangleComponent", UPrimitiveComponent::StaticClass(), &UTriangleComponent::ConstructCubeComponent);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
};