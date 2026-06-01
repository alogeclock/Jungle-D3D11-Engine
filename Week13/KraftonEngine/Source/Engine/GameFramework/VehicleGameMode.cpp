#include "GameFramework/VehicleGameMode.h"

#include "GameFramework/VehiclePawn.h"

AVehicleGameMode::AVehicleGameMode()
{
    DefaultPawnClass = AVehiclePawn::StaticClass();
}

