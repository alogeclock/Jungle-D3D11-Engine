#include "Level.h"

ULevel::ULevel(const FString &InString) : UObject(InString)
{
    LevelName = "PersistentLevel"; }

ULevel::~ULevel() { ClearActors(); }

void ULevel::ClearActors()
{
    // 1. 레벨에 존재하는 모든 액터의 메모리를 해제합니다.
    for (AActor *Actor : Actors)
    {
        if (Actor != nullptr)
        {
            // 이 줄이 실행되면 Actor가 소멸되면서, 내부의 Component들도 알아서 함께 소멸됩니다!
            delete Actor;
        }
    }

    // 2. 액터들을 담고 있던 배열(리스트)을 완전히 비워줍니다.
    Actors.clear();
}

void ULevel::SetLevelName(const FString &levelName) { LevelName = levelName; }
