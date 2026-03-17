#pragma once

#include "Object.h"

class AActor;

class ULevel : public UObject
{
public:
    ULevel();
    TArray<AActor *> &GetActors() { return Actors; }
  void              ClearActors();

  FString GetLevelName();
  void    SetLevelName(FString levelName);

    static UObject   *ConstructULevel() { return new ULevel(); }

    static UClass *StaticClass()
    {
        static UClass s_Class("ULevel", UObject::StaticClass(), &ULevel::ConstructULevel);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }
private:
    TArray<AActor *> Actors;
  FString          LevelName;
};
