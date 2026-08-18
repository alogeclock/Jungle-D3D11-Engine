#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Core/Public/TimeManager.h"

UTimeManager::UTimeManager() {}

UTimeManager::~UTimeManager() {}

void UTimeManager::Initialize()
{
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&startTime);

    DeltaTime = 0.0f;
    TimeAccumulator = 0.0f; 
    FPS = 0.0f;
    FrameCount = 0;
}

void UTimeManager::Update()
{
    QueryPerformanceCounter(&endTime);

    DeltaTime = (double)(endTime.QuadPart - startTime.QuadPart) / frequency.QuadPart;
    if (DeltaTime > 0.2)
        DeltaTime = 0.2;

    startTime = endTime;

    FrameCount++;
    TimeAccumulator += DeltaTime;

    if (TimeAccumulator >= 1.0f)
    {
        FPS = (float)FrameCount;

        FrameCount = 0;
        TimeAccumulator = 0.0f;
    }
}

