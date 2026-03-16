#include "World.h"

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
