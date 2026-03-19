#pragma once

#include "Object\Object.h"
#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class UAxisComponent : public UPrimitiveComponent
{
public:
    UAxisComponent(const FString &InString);
	virtual ~UAxisComponent() override;

		static UObject *Constructor() { return new UAxisComponent("AxisComponentConstructor"); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("UAxisComponent", UPrimitiveComponent::StaticClass(), &UAxisComponent::Constructor);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
};