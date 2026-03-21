#pragma once

#include "Source/Engine/Object/Public/Actor.h"
#include "Source/Engine/Object/Public/Level.h"
#include "Source/Engine/Object/Public/Object.h"
#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"

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
    
    bool SaveLevel(const std::wstring& FilePath);
    bool LoadLevel(const std::wstring& FilePath);

    AActor                  *SpawnActor(UClass *ClassToSpawn);
    template <typename T> T *SpawnActor();
    void                     RemoveActor() const;

    FHitResult PickingRay(const FVector<float> &RayOrigin, const FVector<float> &RayDirection);

    void       Render(URenderer &renderer);

  private:
    ULevel        *CurrentLevel;
    TSet<ULevel *> Levels;
    ULineBatcherComponent *LineBatcher;
};

// ⭐️ 2. 개발자 편의용 템플릿 함수 (회원님이 쓰시던 것)
template <typename T> T *UWorld::SpawnActor()
{
    T* Object = SpawnActor(T::StaticClass());
    Object->SetOuter(this);
    // 내부적으로는 T::StaticClass() 신분증을 꺼내서 1번 함수에게 토스할 뿐입니다!
    return Cast<T>(Object);
}