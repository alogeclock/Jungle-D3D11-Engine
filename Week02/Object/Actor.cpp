#include "Memory/Memory.h"
#include "Actor.h"

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

USceneComponent *AActor::GetRootComponent() const
{
    return RootComponent;
}

void AActor::SetRootComponent(USceneComponent *InOwnedComponents)
{
    RootComponent = InOwnedComponents;
}

void AActor::AddOwnedComponent(UActorComponent *Component)
{
    OwnedComponents.insert(Component);
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
        UPrimitiveComponent *PrimitiveComp = dynamic_cast<UPrimitiveComponent*>(Component);

        if (PrimitiveComp != nullptr)
        {
            PrimitiveComp->Render(renderer);
        }
    }
}
