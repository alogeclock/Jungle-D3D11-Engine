#pragma once

#include "UIPopup.h"
#include "imgui_impl_win32.h"

class FUIPopupBase : public IUIPopup
{
  public:
    void Open() override
    {
        bOpenRequested = true;
        bIsOpen = true;
    }

    void Close() override
    {
        bIsOpen = false;
        bOpenRequested = false;
    }

    bool IsOpen() const override { return bIsOpen; }

    void Update(FGameContext &Context) override {}

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
