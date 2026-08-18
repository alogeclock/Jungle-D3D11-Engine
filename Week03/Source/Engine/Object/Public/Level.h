#pragma once

#include "Object.h"
#include "Actor.h"

class ULevel : public UObject
{
    DECLARE_OBJECT(ULevel, UObject)

  public:
    ULevel(const FString &InString);
    virtual ~ULevel() override;

    void ClearActors();
    TArray<AActor *> &GetActors() { return Actors; }

  private:
    TArray<AActor *> Actors;
};
