#include "Time.h"

void FTime::Reset()
{
	DeltaTime = 0.0f;
	UnscaledDeltaTime = 0.0f;
	TotalTime = 0.0f;
	Fps = 0.0f;
}

void FTime::Update()
{
}

float FTime::GetDeltaTime() const
{
	return DeltaTime * TimeScale;
}

float FTime::GetUnscaledDeltaTime() const
{
	return UnscaledDeltaTime;
}

float FTime::GetTotalTime() const
{
	return TotalTime;
}

float FTime::GetFps() const
{
	return Fps;
}

void FTime::SetTimeScale(float InTimeScale)
{
	TimeScale = InTimeScale;
}

float FTime::GetTimeScale() const
{
	return TimeScale;
}
