#pragma once

#include "Object.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SceneComponent.h"

class AActor : public UObject
{
  private:
    TSet<UActorComponent *> OwnedComponents;
    USceneComponent        *RootComponent = nullptr;

  public:
    AActor(const FString &InString);
    virtual ~AActor() override;

    USceneComponent *GetRootComponent() const;
    void             SetRootComponent(USceneComponent *InOwnedComponents);

    TSet<UActorComponent *> GetOwnedComponents() const { return OwnedComponents; }
    void                    AddOwnedComponent(UActorComponent *Component);

    FTransform GetTransform() const;
    void       SetTransform(const FTransform &NewTransform);

    FTransform GetRotation() const;
    void       GetRotation(const FTransform &NewTransform);

    FTransform GetScale() const;
    void       GetScale(const FTransform &NewTransform);

    void IterateAllActorComponents(URenderer &renderer) const;

    static UObject *Constructor() { return new AActor("ActorConstructor"); }

    static UClass *StaticClass()
    {
        static UClass s_Class("AActor", UObject::StaticClass(), &AActor::Constructor);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }
};
