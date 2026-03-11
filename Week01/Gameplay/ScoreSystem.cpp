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
    float AdditionScore = 0.f;
    switch (Judge)
    {
    case EBeatJudge::Perfect:
        Combo++;
        AdditionScore = 10 * (1 + Combo / 10);
        Score += AdditionScore;
        break;
    case EBeatJudge::Good:
        Combo++;
        AdditionScore = 5 * (1 + Combo / 10);
        Score += AdditionScore;
        break;
    case EBeatJudge::Miss:
        BreakCombo();
        break;
    }

    if (OnJudgeCallback)
    {
        OnJudgeCallback(Judge, AdditionScore, Combo);
    }
}

void FScoreSystem::AddEnemyDefeatBonus() { Score += 500; }

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

void FScoreSystem::SetJudgeCallback(std::function<void(EBeatJudge, float, int)> Callback)
{
    OnJudgeCallback = std::move(Callback);
}
