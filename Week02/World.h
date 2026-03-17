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
    ULevel        *CurrentLevel;
    TSet<ULevel *> Levels;


public:
    UWorld();
    bool    SetCurrentLevel(ULevel *InLevel);
    ULevel *GetCurrentLevel();

    AActor                  *SpawnActor(UClass *ClassToSpawn);
    template <typename T> T *SpawnActor();
    void                     RemoveActor() const;
    
    ULevel *CreateNewLevel();

    virtual UWorld* GetWorld() const override { return const_cast<UWorld *>(this); }

    void Render(URenderer &renderer);
    FHitResult PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);
};

// ⭐️ 2. 개발자 편의용 템플릿 함수 (회원님이 쓰시던 것)
template <typename T> T *UWorld::SpawnActor()
{
    // 내부적으로는 T::StaticClass() 신분증을 꺼내서 1번 함수에게 토스할 뿐입니다!
    return Cast<T>(SpawnActor(T::StaticClass()));
}