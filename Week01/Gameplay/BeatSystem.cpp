#include "pch.h"
#include "BeatSystem.h"
#include <cmath>

void FBeatSystem::Reset()
{
    ElapsedTime = 0.0f;
    bIsBeatJustTriggered = false;
}

void FBeatSystem::Update(float DeltaTime, FGameContext &Context)
{
    float PreviousTime = ElapsedTime;
    ElapsedTime += DeltaTime * TimeScale;

    int PreviousBeat = static_cast<int>(PreviousTime / BeatInterval);
    int CurrentBeat = static_cast<int>(ElapsedTime / BeatInterval);

    bIsBeatJustTriggered = (CurrentBeat > PreviousBeat);
}

void FBeatSystem::SetBpm(float InBpm)
{
    Bpm = InBpm;
    BeatInterval = 60.0f / Bpm;
}

float FBeatSystem::GetBpm() const { return Bpm; }

float FBeatSystem::GetBeatInterval() const { return BeatInterval; }

void FBeatSystem::SetJudgeWindows(float InPerfectWindow, float InGoodWindow)
{
    GoodWindow = InGoodWindow;
}

EBeatJudge FBeatSystem::JudgeInput() const
{
    float TimeToNext = GetTimeToNextBeat();
    float TimeSinceLast = BeatInterval - TimeToNext;
    float Distance = (TimeToNext < TimeSinceLast) ? TimeToNext : TimeSinceLast;

    if (Distance <= GoodWindow)
    {
        return EBeatJudge::Good;
    }
    return EBeatJudge::Miss;
}

bool FBeatSystem::IsBeatJustTriggered() const { return bIsBeatJustTriggered; }

bool FBeatSystem::ConsumeBeat()
{
    if (bIsBeatJustTriggered)
    {
        bIsBeatJustTriggered = false;
        bIsBeatJustConsumed = true;
        return true;
    }
    return false;
}

bool FBeatSystem::IsBeatSkipped()
{
    if (bIsBeatJustConsumed && fmodf(ElapsedTime, BeatInterval) >= GoodWindow)
    {
        bIsBeatJustConsumed = false;
        return true;
    }
    return false;
}

float FBeatSystem::GetElapsedTime() const { return ElapsedTime; }

float FBeatSystem::GetTimeToNextBeat() const
{
    float NextBeatTime = (static_cast<int>(ElapsedTime / BeatInterval) + 1) * BeatInterval;
    return NextBeatTime - ElapsedTime;
}

float FBeatSystem::GetGoodWindow() const { return GoodWindow; }

void FBeatSystem::SetTimeScale(float InScale) { TimeScale = InScale; }

float FBeatSystem::GetTimeScale() const { return TimeScale; }
