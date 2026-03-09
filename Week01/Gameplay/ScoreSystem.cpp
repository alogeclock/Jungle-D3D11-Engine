#include "ScoreSystem.h"

void FScoreSystem::Reset()
{
	Score = 0;
	Combo = 0;
}

void FScoreSystem::AddMoveScore()
{
	Score += 10;
}

void FScoreSystem::AddBeatBonus(EBeatJudge Judge)
{
	switch (Judge)
	{
	case EBeatJudge::Perfect:
		Combo++;
		Score += 50 * Combo;
		break;
	case EBeatJudge::Good:
		Combo++;
		Score += 25 * Combo;
		break;
	case EBeatJudge::Miss:
		BreakCombo();
		break;
	}
}

void FScoreSystem::AddEnemyDefeatBonus()
{
	Score += 100;
}

void FScoreSystem::BreakCombo()
{
	Combo = 0;
}

int FScoreSystem::GetScore() const
{
	return Score;
}

int FScoreSystem::GetCombo() const
{
	return Combo;
}
