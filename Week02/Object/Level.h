#pragma once

#include "Object.h"
#include "Actor.h"

class ULevel : public UObject
{
public:
    ULevel(const FString &InString);
  virtual ~ULevel() override;
  
    TArray<AActor *> &GetActors() { return Actors; }
  void              ClearActors();

  void SetLevelName(const FString &LevelName);

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
