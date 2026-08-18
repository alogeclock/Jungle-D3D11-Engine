#pragma once

#include "GameFramework/GameModeBase.h"
#include "VehicleGameMode.generated.h"

UCLASS(Actor)
class AVehicleGameMode : public AGameModeBase
{
public:
    GENERATED_BODY(AVehicleGameMode)

    AVehicleGameMode();
    ~AVehicleGameMode() override = default;
};

