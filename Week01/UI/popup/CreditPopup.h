#pragma once

#include <vector>
#include "Data/CreditLoader.h"
#include "UIPopupAction.h"
#include "UIPopupBase.h"

struct FGameContext;

class FCreditPopup : public FUIPopupBase
{
  public:
    void SetCredits(const std::vector<FCreditEntry> &InCredits) { Credits = InCredits; }

    EUIPopupAction ConsumeAction();
    void           Render(FGameContext &Context) override;
    void           Update(FGameContext &Context) override {}

  private:
    static constexpr EUIPopupContentAlign    ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Big;

    static constexpr float NameColumnMinWidth = 220.0f;
    static constexpr float PipeGap = 18.0f;
    static constexpr float LineGap = 14.0f;
    static constexpr float BlockOffsetX = -10.0f;
    static constexpr float TopPadding = 6.0f;

  private:
    void DrawCredits(const FPopupFrameLayout &Layout);

  private:
    std::vector<FCreditEntry> Credits;
    EUIPopupAction            PendingAction = EUIPopupAction::None;
};
