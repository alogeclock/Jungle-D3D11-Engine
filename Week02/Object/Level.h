#pragma once

#include "Object.h"

class AActor;

class ULevel : public UObject
{
public:
    TArray<AActor *> &GetActors() { return Actors; }

  private:
    TArray<AActor *> Actors;
};
