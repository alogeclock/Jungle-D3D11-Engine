#pragma once

#include "Object/Level.h"
#include "Object/Object.h"

class ULevel;
struct FHitResult;

class UWorld final : public UObject
{
public:
    ULevel        *CurrentLevel;
    TSet<ULevel *> Levels;


public:
    UWorld();
    bool    SetCurrentLevel(ULevel *InLevel);
    ULevel *GetCurrentLevel();

    template <typename T> T *SpawnActor();
    void                     RemoveActor() const;
    
    ULevel *CreateNewLevel();

    virtual UWorld* GetWorld() const override { return const_cast<UWorld *>(this); }

    void Render(URenderer &renderer);
    FHitResult PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);
};

template <typename T> T *UWorld::SpawnActor()
{
    T *NewActor = new T();

    if (CurrentLevel)
    {
        NewActor->SetOuter(CurrentLevel);
        CurrentLevel->GetActors().push_back(NewActor);
    }

    return NewActor;
}