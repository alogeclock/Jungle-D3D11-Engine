#include "ScoreSystem.h"

void FScoreSystem::Reset()
{
    Score = 0;
    Combo = 0;
}

void FScoreSystem::AddMoveScore() { Score += 10; }

void FScoreSystem::AddBeatBonus(EBeatJudge Judge)
{
    switch (Judge)
    {
    case EBeatJudge::Good:
        Combo++;
        Score += 25 * Combo;
        break;
    case EBeatJudge::Miss:
        BreakCombo();
        break;
    }
}

void FScoreSystem::AddEnemyDefeatBonus() { Score += 100; }

void FScoreSystem::AddTimeBonus(float RemainingSeconds)
{
	int Bonus = static_cast<int>(RemainingSeconds * 50);
	if (Bonus > 0)
	{
		Score += Bonus;
	}
}

void FScoreSystem::BreakCombo() { Combo = 0; }

int FScoreSystem::GetScore() const { return Score; }

int FScoreSystem::GetCombo() const { return Combo; }
