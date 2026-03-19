#pragma once

#include "Object/Object.h"

class AActor;

// 가장 기본이 되는 컴포넌트
class UActorComponent : public UObject
{
  public:
    UActorComponent(const FString &InString);
    virtual ~UActorComponent() override;

    void RegisterComponent();
    virtual void InitializeComponent();
    virtual void TickComponent(float DeltaTime);

    AActor *GetOwner() const;
    void SetOwner(AActor* InOwner) { Owner = InOwner; }

    static UObject *Constructor() { return new UActorComponent("ConstructActorComponent"); }

    static UClass *StaticClass()
    {
        // 부모를 UPrimitiveComponent::StaticClass() 로 지정
        static UClass s_Class("UActorComponent", UObject::StaticClass(), &UActorComponent::Constructor);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

  protected:
    AActor *Owner = nullptr;
};