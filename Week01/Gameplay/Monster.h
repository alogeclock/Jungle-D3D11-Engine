#pragma once

#include "Actor.h"

class FMonster : public FActor
{
public:
	EActorType GetActorType() const override;

	void Update(float DeltaTime) override;
	void OnBeat(FStage& Stage) override;

	void SetAiType(EMonsterAIType InAiType);
	EMonsterAIType GetAiType() const;

	EDirection DecideNextMove(const FStage& Stage) const;

private:
	EMonsterAIType AiType = EMonsterAIType::RandomMove;
};
