#pragma once

class FTime
{
public:
	void Reset();
	void Update();

	float GetDeltaTime() const;
	float GetUnscaledDeltaTime() const;
	float GetTotalTime() const;
	float GetFps() const;

	void SetTimeScale(float InTimeScale);
	float GetTimeScale() const;

private:
	float DeltaTime = 0.0f;
	float UnscaledDeltaTime = 0.0f;
	float TotalTime = 0.0f;
	float TimeScale = 1.0f;
	float Fps = 0.0f;
};
