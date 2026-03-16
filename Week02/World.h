#pragma once

#include "Object/Level.h"
#include "Object/Object.h"

class ULevel;

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