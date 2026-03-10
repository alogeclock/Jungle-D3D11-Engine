#pragma once

#include "GameContext.h"

class IUIPopup
{
  public:
    virtual ~IUIPopup() = default;

    virtual void Update(FGameContext &Context) = 0;
    virtual void Render(FGameContext &Context) = 0;

    bool IsClosed() const { return bClosed; }

  protected:
    void Close() { bClosed = true; }

  private:
    bool bClosed = false;
};
