#pragma once

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include <iostream>
class USphereComponent : public UPrimitiveComponent
{
public:
    virtual UClass* GetClass() const override
    {
        return USphereComponent::StaticClass();
    }

private:
    static UObject* Constructor()
    {
        return new USphereComponent("USphereComponent");
    }
    inline static UClass* _StaticUClass = nullptr;

public:
    static UClass* StaticClass()
    {
        static UClass* s_Class = []() -> UClass* {
            UClass* cls = new UClass("USphereComponent", UPrimitiveComponent::StaticClass(), sizeof(USphereComponent),
                                     &USphereComponent::Constructor);
            UClass* SuperClass = UPrimitiveComponent::StaticClass();
            TArray<FProperty>& Properties = SuperClass->GetProperties();
            int SuperPropertyCount = Properties.size();
            for (int i = 0; i < SuperPropertyCount; i++)
            {
                if (Properties[i].bIsPublic)
                    cls->AddProperty(Properties[i]);
            }
            // 내 클래스, 해당 멤버 변수 이름, 해당 멤버 자료형
            cls->AddProperty(
                {"SphereRadius", EPropertyType::Float,
                 ((::size_t) & reinterpret_cast<char const volatile&>((((USphereComponent*)0)->SphereRadius))), false});
            std::cout << cls->GetProperties().size() << std::endl;
            return cls;
        }();
        return s_Class;
    }

private:
public:
    USphereComponent(const FString& InString, float inSphereRadius = 1.0f);
    virtual ~USphereComponent() override;

    virtual void Submit() override;

protected:
    float SphereRadius = 1.0f;
};