#include "Memory/Memory.h"
#include "Actor.h"

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

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

USceneComponent *AActor::GetRootComponent() const {
    return RootComponent;
}

void AActor::SetRootComponent(USceneComponent *InOwnedComponents)
{
    RootComponent = InOwnedComponents;
}

void AActor::AddOwnedComponent(UActorComponent *Component)
{
    if (Component == nullptr)
        return;

    OwnedComponents.insert(Component);
    
    if (RootComponent == nullptr)
        return;
    
    USceneComponent *SceneComponent = Cast<USceneComponent>(Component);

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

void AActor::SetTransform(const FTransform &NewTransform)
{
    if (RootComponent)
    {
        RootComponent->SetTransform(NewTransform);
    }
}

void AActor::IterateAllActorComponents(URenderer &renderer) const
{
    // Actor의 GetComponents()는 보통 컴포넌트들의 Set이나 Array를 반환합니다.
    for (UActorComponent *Component : OwnedComponents)
    {
        UPrimitiveComponent *PrimitiveComp = Cast<UPrimitiveComponent>(Component);

        if (PrimitiveComp != nullptr)
        {
            PrimitiveComp->Render(renderer);
        }
    }
}