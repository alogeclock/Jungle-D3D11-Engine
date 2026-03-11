#pragma once

#include "UIPopupAction.h"
#include "UIPopupBase.h"

class FGameOverPopup : public FUIPopupBase
{
  public:
    EUIPopupAction ConsumeAction();
    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    static constexpr EUIPopupContentAlign ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Big;

  private:
    EUIPopupAction PendingAction = EUIPopupAction::None;
};
