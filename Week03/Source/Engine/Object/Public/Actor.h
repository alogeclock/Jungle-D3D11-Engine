#pragma once

#include "Object.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Engine/Public/Classes/Components/SceneComponent.h"

class AActor : public UObject
{
    DECLARE_OBJECT_START(AActor, UObject)
    PUBLIC_PROPERTY(AActor, RootComponent, UObjectDetail)
    PUBLIC_PROPERTY(AActor, OwnedComponents, UObjectPtrArray)
    DECLARE_END
  private:
    TArray<UActorComponent *> OwnedComponents;
    USceneComponent        *RootComponent = nullptr;

  public:
    AActor(const FString &InString);
    virtual ~AActor() override;

    USceneComponent *GetRootComponent() const;
    void             SetRootComponent(USceneComponent *InOwnedComponents);

    TArray<UActorComponent *> GetOwnedComponents() const { return OwnedComponents; }
    void                    AddOwnedComponent(UActorComponent *Component);

    FTransform GetTransform() const;
    void       SetTransform(const FTransform &NewTransform);

    virtual void Tick(float deltaTime);

    void IterateAllActorComponents(URenderer &renderer) const;
};
