#pragma once

#include "UIPopupAction.h"
#include "UIPopupBase.h"

class FGoToTitlePopup : public FUIPopupBase
{
  public:
    EUIPopupAction ConsumeAction();

    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    EUIPopupAction PendingAction = EUIPopupAction::None;
};
