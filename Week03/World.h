#pragma once

#include "Object/Actor.h"
#include "Object/Level.h"
#include "Object/Object.h"

class ULevel;
class AActor;
struct FHitResult;

class UWorld final : public UObject
{
  public:
    UWorld(const FString &InString);
    ~UWorld();
    
    virtual UWorld *GetWorld() const override { return const_cast<UWorld *>(this); }

    ULevel *CreateNewLevel(const FString &NewLevelName);

    ULevel *GetCurrentLevel() { return CurrentLevel; }
    
    bool SaveLevel(const FString& FilePath);
    bool LoadLevel(const FString& FilePath);

    AActor                  *SpawnActor(UClass *ClassToSpawn);
    template <typename T> T *SpawnActor();
    void                     RemoveActor() const;

    FHitResult PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

    static UObject *Constructor() { return new UWorld("WorldConstructor"); }

    static UClass *StaticClass()
    {
        static UClass s_Class("UWorld", UObject::StaticClass(), &UWorld::Constructor);
        return &s_Class;
    }

    virtual UClass *GetClass() const override { return StaticClass(); }

    void       Render(URenderer &renderer);

  private:
    ULevel        *CurrentLevel;
    TSet<ULevel *> Levels;    
};

// ⭐️ 2. 개발자 편의용 템플릿 함수 (회원님이 쓰시던 것)
template <typename T> T *UWorld::SpawnActor()
{
    // 내부적으로는 T::StaticClass() 신분증을 꺼내서 1번 함수에게 토스할 뿐입니다!
    return Cast<T>(SpawnActor(T::StaticClass()));
}