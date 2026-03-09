#pragma once

#include "../Core/Types.h"

class FScoreSystem
{
public:
	void Reset();

	void AddMoveScore();
	void AddBeatBonus(EBeatJudge Judge);
	void AddEnemyDefeatBonus();

	void BreakCombo();

	int GetScore() const;
	int GetCombo() const;

private:
	int Score = 0;
	int Combo = 0;
};
