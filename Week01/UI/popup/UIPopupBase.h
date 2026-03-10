#pragma once

#include "pch.h"

class FUIPopupBase
{
  public:
    void Open()
    {
        bOpenRequested = true;
        bIsOpen = true;
    }

    void Close()
    {
        bIsOpen = false;
        bOpenRequested = false;
    }

    bool IsOpen() const { return bIsOpen; }
    bool IsClosed() const { return !bIsOpen; }

    virtual void Update(FGameContext &Context) = 0;
    virtual void Render(FGameContext &Context) = 0;

  protected:
    bool ConsumeOpenRequest()
    {
        if (!bOpenRequested)
            return false;

        bOpenRequested = false;
        return true;
    }

  protected:
    bool bIsOpen = false;
    bool bOpenRequested = false;
};
