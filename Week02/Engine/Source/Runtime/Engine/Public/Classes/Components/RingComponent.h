#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class URingComponent : public UPrimitiveComponent
{
public:
    URingComponent(const FString &InString);
	virtual ~URingComponent() override;

		static UObject *Constructor() { return new URingComponent("RingComponentConstructor"); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("URingComponent", UPrimitiveComponent::StaticClass(), &URingComponent::Constructor);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
};