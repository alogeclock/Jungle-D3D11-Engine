#include "World.h"
#include "Object/Actor.h"

UWorld::UWorld()
{
    CreateNewLevel();
}

ULevel *UWorld::CreateNewLevel()
{
    ULevel *NewLevel = new ULevel();

    NewLevel->SetOuter(this);

    Levels.insert(NewLevel);

	CurrentLevel = NewLevel;

	return NewLevel;
}

void UWorld::Render(URenderer &renderer)
{
    if (CurrentLevel)
    {
        for (AActor* actor : CurrentLevel->GetActors())
        {
            actor->IterateAllActorComponents(renderer);
        }
    }
}
