#pragma once

#include "CoreTypes.h"

class UEngineStatics
{
  public:
    static uint32 GenUUID() { return NextUUID++; }
    static void SetUUID(uint32 InNumber)
    {
        NextUUID = InNumber;
    }

  private:
    static uint32 NextUUID;
};