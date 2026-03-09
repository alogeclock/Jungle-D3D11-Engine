#include "Monster.h"
#include "Stage.h"
#include <cstdlib>

EActorType FMonster::GetActorType() const
{
	return EActorType::Monster;
}

void FMonster::Update(float DeltaTime)
{
	FActor::Update(DeltaTime);
}

void FMonster::OnBeat(FStage& Stage)
{
	EDirection Dir = DecideNextMove(Stage);
	TryMove(Stage, Dir);
}

void FMonster::SetAiType(EMonsterAIType InAiType)
{
	AiType = InAiType;
}

EMonsterAIType FMonster::GetAiType() const
{
	return AiType;
}

EDirection FMonster::DecideNextMove(const FStage& Stage) const
{
	// Default: random move
	int Random = std::rand() % 4;
	switch (Random)
	{
	case 0: return EDirection::Up;
	case 1: return EDirection::Down;
	case 2: return EDirection::Left;
	default: return EDirection::Right;
	}
}
