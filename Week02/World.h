#pragma once

#include "Object/Level.h"
#include "Object/Object.h"

class ULevel;
struct FHitResult;

class UWorld final : public UObject
{
  public:
    UWorld();
    ~UWorld();
    
    virtual UWorld *GetWorld() const override { return const_cast<UWorld *>(this); }

    ULevel *GetCurrentLevel();

    bool SaveLevel(const std::string& FilePath);
    bool LoadLevel(const std::string& FilePath);

    template <typename T> T *SpawnActor();
    void                     RemoveActor() const;

    ULevel *CreateNewLevel();

    static UObject *ConstructWorld() { return new UWorld(); }

    static UClass *StaticClass()
    {
        static UClass s_Class("UWorld", UObject::StaticClass(), &UWorld::ConstructWorld);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

    void       Render(URenderer &renderer);
    FHitResult PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

  private:
  public:
    ULevel        *CurrentLevel;
    TSet<ULevel *> Levels;
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