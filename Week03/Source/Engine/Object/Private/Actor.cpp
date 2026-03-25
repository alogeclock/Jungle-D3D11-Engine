#include "../Public/Actor.h"
#include "Source/Core/Public/Memory.h"

#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include <iostream>

AActor::AActor(const FString& InString) : UObject(InString)
{
}

AActor::~AActor()
{
     for (UActorComponent *Component : OwnedComponents)
    {
         if (Component != nullptr)
         {
             delete Component;
         }
     }

    // 2. 컨테이너 비우기 및 포인터 초기화
    OwnedComponents.clear();
    RootComponent = nullptr;
}

USceneComponent* AActor::GetRootComponent() const
{
    return RootComponent;
}

void AActor::SetRootComponent(USceneComponent* InOwnedComponents)
{
    RootComponent = InOwnedComponents;
}

void AActor::AddOwnedComponent(UActorComponent* Component)
{
    if (Component == nullptr)
        return;

    OwnedComponents.push_back(Component);

    if (RootComponent == nullptr)
        return;

    USceneComponent* SceneComponent = Cast<USceneComponent>(Component);

    if (SceneComponent == RootComponent || SceneComponent == nullptr)
        return;

    SceneComponent->SetupAttachment(RootComponent);
}

FTransform AActor::GetTransform() const
{
    if (RootComponent)
    {
        return RootComponent->GetTransform();
    }

    return FTransform();
}

void AActor::SetTransform(const FTransform& NewTransform)
{
    if (RootComponent)
    {
        RootComponent->SetTransform(NewTransform);
    }
}

void AActor::Tick(float deltaTime)
{
    if (!bIsActive)
    {
        return;
    }

    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component != nullptr && Component->IsActive())
        {
            Component->Tick(deltaTime);
        }
    }
}

// 렌더러에 모든 Components의 데이터를 전송한다.
void AActor::SubmitAllActorComponents(const FSceneViewOptions& ViewOptions) const
{
    if (!bIsActive)
    {
        return;
    }

    for (UActorComponent* Comp : this->GetOwnedComponents())
    {
        if (Comp == nullptr || !Comp->IsActive())
        {
            continue;
        }

        UPrimitiveComponent* PrimitiveComp = Cast<UPrimitiveComponent>(Comp);

        if (PrimitiveComp != nullptr)
        {
            PrimitiveComp->Submit(ViewOptions);
        }
    }
}
