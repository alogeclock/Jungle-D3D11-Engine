#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class USphereComponent : public UPrimitiveComponent
{
public:
    USphereComponent(const FString &InString, float inSphereRadius = 1.0f);
	virtual ~USphereComponent() override;

		static UObject *Constructor() { return new USphereComponent("SphereComponentConstructor"); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("USphereComponent", UPrimitiveComponent::StaticClass(), &USphereComponent::Constructor);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
	float SphereRadius = 1.0f;
};