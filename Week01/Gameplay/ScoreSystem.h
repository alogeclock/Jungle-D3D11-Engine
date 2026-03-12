#pragma once

#include "Core/Types.h"
#include <functional>
#include "Core/GameContext.h"

class FScoreSystem
{
  public:
    void Reset();

    void AddBeatBonus(EBeatJudge Judge);
    void AddEnemyDefeatBonus();
    void AddTimeBonus(float RemainingTime, float TimeLimit);

    void BreakCombo();

    int  GetScore() const;
    void SetScore(int InScore);
    int  GetCombo() const;

    void SetJudgeCallback(std::function<void(EBeatJudge, float, int)> Callback);
    void SetJudgeScoreUpdateCallback(std::function<void(int)> Callback);
    void SetTimerBonusCallback(std::function<void(int)> Callback);

  private:
    int                                         Score = 0;
    int                                         Combo = 0;
    std::function<void(EBeatJudge, float, int)> OnJudgeCallback;
    std::function<void(int)>                    OnJudgeScoreUpdateCallback;
    std::function<void(int)>                    OnTimerBonusCallback;
};
