#pragma once

#include <string>
#include "UIPopupBase.h"
#include "UIPopupAction.h"

struct FGameContext;

class FStageIntroPopup : public FUIPopupBase
{
  public:
    void SetStageNumber(int InStageNumber) { StageNumber = InStageNumber; }
    void SetMessage(const std::string &InMessage) { Message = InMessage; }

    void SetData(int InStageNumber, const std::string &InMessage)
    {
        StageNumber = InStageNumber;
        Message = InMessage;
    }

    EUIPopupAction ConsumeAction();

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    std::string GetPopupTitle() const;

  private:
    int             StageNumber = 1;
    std::string     Message = "Press OK to begin.";
    EUIPopupAction  PendingAction = EUIPopupAction::None;

  private:
    static constexpr EUIPopupContentAlign ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Medium;
    static constexpr EUIPopupContentVerticalAlign ContentVerticalAlign =
        EUIPopupContentVerticalAlign::Center;
    static constexpr float MessageLineGap = 14.0f;
};
