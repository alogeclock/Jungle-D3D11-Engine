#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UCubeArrowComponent : public UPrimitiveComponent
{
public:
	UCubeArrowComponent() ;
	virtual ~UCubeArrowComponent() override;

		static UObject *ConstructCubeComponent() { return new UCubeArrowComponent(); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("UCubeArrowComponent", UPrimitiveComponent::StaticClass(), &UCubeArrowComponent::ConstructCubeComponent);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
};