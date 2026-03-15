#pragma once

#include "Object/Level.h"
#include "Object/Object.h"

class ULevel;

class World final : public UObject
{
public:
    ULevel *CurrentLevel;

    template<typename T> T *SpawnActor();

    virtual World *GetWorld() const override { return const_cast<World *>(this); }
};

template <typename T> T *World::SpawnActor()
{
    T *NewActor = new T();

    if (CurrentLevel)
    {
        CurrentLevel->Actors.push_back(NewActor);
    }

    return NewActor;
}