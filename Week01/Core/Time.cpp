#include "Time.h"

void FTime::Reset()
{
    DeltaTime = 0.0f;
    UnscaledDeltaTime = 0.0f;
    TotalTime = 0.0f;
    Fps = 0.0f;
    TimeScale = 1.0f;

    FpsElapsedTime = 0.0f;
    FrameCount = 0;
    bIsInitialized = false;

    Frequency = {};
    PreviousCounter = {};
}

/*
    Delta Time
    - Unscaled: 실제로 흐른 시간 -> UI, 시스템 타이머
    - Scaled: 게임 내에서 흐른 시간 -> 게임 내 로직 (물리 연산, 애니메이션 등)
*/
void FTime::Update()
{
    if (!bIsInitialized)
    {
        QueryPerformanceFrequency(&Frequency);
        QueryPerformanceCounter(&PreviousCounter);
        bIsInitialized = true;
        return;
    }

    // 현재 카운터 구하기
    LARGE_INTEGER CurrentCounter{};
    QueryPerformanceCounter(&CurrentCounter);

    // delta 계산
    const LONGLONG CounterDelta = CurrentCounter.QuadPart - PreviousCounter.QuadPart;
    UnscaledDeltaTime = static_cast<float>(CounterDelta) / static_cast<float>(Frequency.QuadPart);
    PreviousCounter = CurrentCounter;

    // 프레임당 delta 최대값 제한
    if (UnscaledDeltaTime > MaxDeltaTime)
        UnscaledDeltaTime = MaxDeltaTime;

    DeltaTime = UnscaledDeltaTime * TimeScale;
    TotalTime += DeltaTime;

    // FPS 계산
    ++FrameCount;
    FpsElapsedTime += UnscaledDeltaTime;

    if (FpsElapsedTime >= 1.0f)
    {
        Fps = static_cast<float>(FrameCount) / FpsElapsedTime;
        FpsElapsedTime = 0.0f;
        FrameCount = 0;
    }
}

float FTime::GetDeltaTime() const { return DeltaTime * TimeScale; }

float FTime::GetUnscaledDeltaTime() const { return UnscaledDeltaTime; }

float FTime::GetTotalTime() const { return TotalTime; }

float FTime::GetFps() const { return Fps; }

void FTime::SetTimeScale(float InTimeScale) { TimeScale = InTimeScale; }

float FTime::GetTimeScale() const { return TimeScale; }
