#pragma once

#include "Object.h"

class AActor;

class ULevel : public UObject
{
public:
    TArray<AActor *> &GetActors() { return Actors; }
  void              ClearActors();

    static UObject   *ConstructULevel() { return new ULevel(); }

    static UClass *StaticClass()
    {
        static UClass s_Class("ULevel", UObject::StaticClass(), &ULevel::ConstructULevel);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }
private:
    TArray<AActor *> Actors;
};
