#pragma once

#include <Windows.h>

// todo: 디폴트값 없애기?
class FTime
{
  private:
    static constexpr float MaxDeltaTime = 0.05f;

  public:
    void Reset();
    void Update();

    float GetDeltaTime() const;
    float GetUnscaledDeltaTime() const;
    float GetTotalTime() const;
    float GetFps() const;

    void  SetTimeScale(float InTimeScale);
    float GetTimeScale() const;

  private:
    float DeltaTime = 0.0f;
    float UnscaledDeltaTime = 0.0f;
    float TotalTime = 0.0f;
    float TimeScale = 1.0f; // 슬로우 모션 등의 효과 적용 가능 (확장)

    float Fps = 0.0f;

    LARGE_INTEGER Frequency{};       // 고해상도 타이머가 1초에 몇 번 카운트되는지
    LARGE_INTEGER PreviousCounter{}; //  이전 프레임 시각

    float FpsElapsedTime = 0.0f;
    int   FrameCount = 0;
    bool  bIsInitialized = false;
};
