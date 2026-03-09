#pragma once

#include "../Core/Types.h"

class FBeatSystem
{
public:
	void Reset();
	void Update(float DeltaTime);

	void SetBpm(float InBpm);
	float GetBpm() const;
	float GetBeatInterval() const;

	void SetJudgeWindows(float InPerfectWindow, float InGoodWindow);

	EBeatJudge JudgeInput() const;

	bool IsBeatJustTriggered() const;
	bool ConsumeBeat();

	float GetElapsedTime() const;
	float GetTimeToNextBeat() const;

private:
	float Bpm = 120.0f;
	float BeatInterval = 0.5f;
	float ElapsedTime = 0.0f;

	float PerfectWindow = 0.08f;
	float GoodWindow = 0.15f;

	bool bIsBeatJustTriggered = false;
};
