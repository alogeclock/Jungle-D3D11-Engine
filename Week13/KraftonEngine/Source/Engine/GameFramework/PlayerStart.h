#pragma once

#include "GameFramework/AActor.h"
#include "PlayerStart.generated.h"

UCLASS(Actor)
class APlayerStart : public AActor
{
public:
	GENERATED_BODY(APlayerStart)

	APlayerStart() = default;
	~APlayerStart() override = default;

	void InitDefaultComponents();
};
