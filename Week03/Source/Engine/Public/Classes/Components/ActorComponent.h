#pragma once

#include "Source/Engine/Object/Public/Object.h"

class AActor;

// 가장 기본이 되는 컴포넌트
class UActorComponent : public UObject
{
    DECLARE_OBJECT(UActorComponent, UObject)

  public:
    UActorComponent(const FString &InString);
    virtual ~UActorComponent() override;

    void RegisterComponent();
    virtual void InitializeComponent();
    virtual void TickComponent(float DeltaTime);

    virtual void Tick(float deltaTime);

    AActor *GetOwner() const;
    void SetOwner(AActor* InOwner) { Owner = InOwner; }
    void SetActive(bool bInIsActive) { bIsActive = bInIsActive; }
    bool IsActive() const { return bIsActive; }

  protected:
    AActor *Owner = nullptr;
    bool    bIsActive = true;
};
