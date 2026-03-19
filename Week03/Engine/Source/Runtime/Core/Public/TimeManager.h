#pragma once

#include <windows.h>

class UTimeManager
{
public:
    static UTimeManager &Get()
    {
        static UTimeManager instance;
        return instance;
    }

    UTimeManager();
    ~UTimeManager();

public:
  void  Initialize();
  void  Update();
  double GetDeltaTime() { return DeltaTime; }
  float GetFPS() { return FPS; }
  double GetFrameTime() { return GetDeltaTime() * 1000.0f; }

private:
  LARGE_INTEGER frequency;
  LARGE_INTEGER startTime;
  LARGE_INTEGER endTime;

  double DeltaTime;
  double TimeAccumulator; 

  float FPS;
  int   FrameCount = 0;
};