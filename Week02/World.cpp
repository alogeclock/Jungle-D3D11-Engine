#include "World.h"

UWorld::UWorld()
{
    CreateNewLevel();
}

ULevel *UWorld::CreateNewLevel()
{
    ULevel *NewLevel = new ULevel();

    Levels.insert(NewLevel);

	CurrentLevel = NewLevel;

	return NewLevel;
}
