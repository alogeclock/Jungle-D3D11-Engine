#include "Memory/Memory.h"
#include "Actor.h"

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