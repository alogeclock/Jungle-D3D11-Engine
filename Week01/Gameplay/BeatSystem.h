#pragma once

#include "Core/Types.h"

struct FGameContext;

class FBeatSystem
{
  public:
    void Reset();
    void Update(float DeltaTime, FGameContext &Context);

    void  SetBpm(float InBpm);
    float GetBpm() const;
    float GetBeatInterval() const;

    void SetJudgeWindows(float InPerfectWindow, float InGoodWindow);

    EBeatJudge JudgeInput() const;

    bool IsBeatJustTriggered() const;
    bool ConsumeBeat();
    bool IsBeatSkipped();

    float GetElapsedTime() const;
    float GetTimeToNextBeat() const;
    float GetGoodWindow() const;

  private:
    float Bpm = 120.0f;
    float BeatInterval = 0.5f;
    float ElapsedTime = 0.0f;

    float PerfectWindow = 0.08f;
    float GoodWindow = 0.2f;

    bool bIsBeatJustTriggered = false;
    bool bIsBeatJustConsumed = false;
};
