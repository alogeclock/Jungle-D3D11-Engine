#pragma once

#include "Object.h"

class AActor;

class ULevel : public UObject
{
public:
    ULevel(const FString &InString);
    TArray<AActor *> &GetActors() { return Actors; }
  void              ClearActors();

  FString GetLevelName();
  void    SetLevelName(FString levelName);

    static UObject *Constructor() { return new ULevel("LevelConstructor"); }

    static UClass *StaticClass()
    {
        static UClass s_Class("ULevel", UObject::StaticClass(), &ULevel::Constructor);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }
private:
    TArray<AActor *> Actors;
  FString          LevelName;
};
