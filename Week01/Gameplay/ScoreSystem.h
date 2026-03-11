#pragma once

#include "Core/Types.h"
#include <functional>

class FScoreSystem
{
  public:
    void Reset();

    void AddMoveScore();
    void AddBeatBonus(EBeatJudge Judge);
    void AddEnemyDefeatBonus();
    void AddTimeBonus(float RemainingTime, float TimeLimit);

    void BreakCombo();

    int  GetScore() const;
    void SetScore(int InScore);
    int  GetCombo() const;

    void SetJudgeCallback(std::function<void(EBeatJudge)> Callback);

  private:
    int                             Score = 0;
    int                             Combo = 0;
    std::function<void(EBeatJudge)> OnJudgeCallback;
};
