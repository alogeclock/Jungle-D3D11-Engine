#pragma once

#include <string>
#include <vector>
#include "UIPopupAction.h"
#include "UIPopupBase.h"

struct FCreditEntry
{
    std::string Name;
    std::string Role;
};

class FCreditPopup : public FUIPopupBase
{
  public:
    void SetCredits(const std::vector<FCreditEntry> &InCredits) { Credits = InCredits; }

    EUIPopupAction ConsumeAction();
    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    static constexpr EUIPopupContentAlign ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Big;
    static constexpr float PipeGap = 18.0f;
    static constexpr float LineGap = 14.0f;
    static constexpr float BlockOffsetX = -10.0f;

  private:
    void DrawCredits(const FPopupFrameLayout &Layout);

  private:
    std::vector<FCreditEntry> Credits = {
        {"KIM YEONHA", "GAME DESIGN"},
        {"LEE HOJIN", "GAMEPLAY PROGRAMMING"},
        {"SIM WOOJIN", "UI DESIGN"},
        {"JEON HYUNGIL", "LEVEL DESIGN"}};

    EUIPopupAction PendingAction = EUIPopupAction::None;
};
