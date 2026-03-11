#include "pch.h"
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
    case EBeatJudge::Perfect:
        Combo++;
        Score += 10;
        break;
    case EBeatJudge::Good:
        Combo++;
        Score += 5;
        break;
    case EBeatJudge::Miss:
        BreakCombo();
        break;
    }

    if (OnJudgeCallback)
    {
        OnJudgeCallback(Judge);
    }
}

void FScoreSystem::AddEnemyDefeatBonus() { Score += 100; }

void FScoreSystem::AddTimeBonus(float RemainingTime, float TimeLimit)
{
    if (TimeLimit <= 0.0f)
        return;
    float Ratio = RemainingTime / TimeLimit;
    Score += static_cast<int>(Ratio * 500.0f);
}

void FScoreSystem::BreakCombo() { Combo = 0; }

int FScoreSystem::GetScore() const { return Score; }

void FScoreSystem::SetScore(int InScore) { Score = InScore; }

int FScoreSystem::GetCombo() const { return Combo; }

void FScoreSystem::SetJudgeCallback(std::function<void(EBeatJudge)> Callback)
{
    OnJudgeCallback = std::move(Callback);
}
