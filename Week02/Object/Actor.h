#pragma once

#include "Object.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SceneComponent.h"

class AActor : public UObject
{
private:
    TSet<UActorComponent *> OwnedComponents;
  USceneComponent        *RootComponent = nullptr;

public:
    USceneComponent *GetRootComponent() const;
    void SetRootComponent(USceneComponent *InOwnedComponents);

    TSet<UActorComponent *> GetOwnedComponents() const { return OwnedComponents; }
    void                    AddOwnedComponent(UActorComponent *Component);

    FTransform GetTransform() const;
    void       SetTransform(const FTransform &NewTransform);

    FTransform GetRotation() const;
    void       GetRotation(const FTransform &NewTransform);

    FTransform GetScale() const;
    void       GetScale(const FTransform &NewTransform);

    void IterateAllActorComponents(URenderer &renderer) const;
};
