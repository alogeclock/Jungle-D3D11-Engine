#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

class USphereComponent : public UPrimitiveComponent
{
public:
	USphereComponent(float inSphereRadius = 1.0f);
	virtual ~USphereComponent() override;

		static UObject *ConstructCubeComponent() { return new USphereComponent(); }

        static UClass *StaticClass()
        {
            // 부모를 UPrimitiveComponent::StaticClass() 로 지정
            static UClass s_Class("USphereComponent", UPrimitiveComponent::StaticClass(), &USphereComponent::ConstructCubeComponent);
            return &s_Class;
        }

        virtual UClass *GetClass() const override { return StaticClass(); }

      protected:
	float SphereRadius = 1.0f;
};