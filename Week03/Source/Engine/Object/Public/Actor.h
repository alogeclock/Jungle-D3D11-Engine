#pragma once

#include "Object.h"
#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Engine/Public/Classes/Components/SceneComponent.h"

class AActor : public UObject
{
    DECLARE_OBJECT(AActor, UObject)
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

    template<typename T>
    T* FindComponentByClass();

    void Tick(float deltaTime);

    void IterateAllActorComponents(URenderer &renderer) const;
};

template <typename T> inline T *AActor::FindComponentByClass()
{
    for (UActorComponent* Comp : OwnedComponents)
    {
        if (T* Found = Cast<T>(Comp))
            return Found;
    }
    return nullptr;
}
