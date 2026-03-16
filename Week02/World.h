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
    ULevel *GetCurrentLevel() const;

    template <typename T> T *SpawnActor();
    void                     RemoveActor() const;
    
    ULevel *CreateNewLevel();

    virtual UWorld* GetWorld() const override { return const_cast<UWorld *>(this); }
};

template <typename T> T *UWorld::SpawnActor()
{
    T *NewActor = new T();

    if (CurrentLevel)
    {
        CurrentLevel->Actors.push_back(NewActor);
    }

    return NewActor;
}