#pragma once

#include "CoreTypes.h"

class UEngineStatics
{
  public:
    static uint32 GenUUID() { return NextUUID++; }
    static void SetUUID(uint32 InNumber = 27)
    {
        NextUUID = InNumber;
    }

  private:
    static uint32 NextUUID;
};