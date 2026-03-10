#pragma once

#include "../Core/GameContext.h"

class IUIWidget
{
  public:
    virtual ~IUIWidget() = default;

    virtual void Update(FGameContext &Context) = 0;
    virtual void Render(FGameContext &Context) = 0;

    void SetVisible(bool bVisible) { bIsVisible = bVisible; }
    bool IsVisible() const { return bIsVisible; }

  protected:
    bool bIsVisible = true;
};
