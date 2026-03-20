#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UCubeArrowComponent : public UPrimitiveComponent
{
public:
    UCubeArrowComponent(const FString &InString);
	virtual ~UCubeArrowComponent() override;

		static UObject *Constructor() { return new UCubeArrowComponent("CubeArrowComponentConstructor"); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("UCubeArrowComponent", UPrimitiveComponent::StaticClass(), &UCubeArrowComponent::Constructor);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
};