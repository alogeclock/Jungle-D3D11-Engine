#pragma once

#include <string>
#include <vector>
#include "UIPopupBase.h"

struct FGameContext;

class FStageIntroPopup : public FUIPopupBase
{
  public:
    void SetData(int InStageNumber, const std::string &InMessage)
    {
        StageNumber = InStageNumber;
        Message = InMessage;
    }

    void SetStageNumber(int InStageNumber) { StageNumber = InStageNumber; }
    void SetMessage(const std::string &InMessage) { Message = InMessage; }

    EUIPopupAction ConsumeAction();

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    std::string              GetPopupTitle() const;
    std::vector<std::string> SplitMessageLines() const;

  private:
    int            StageNumber = 1;
    std::string    Message;
    EUIPopupAction PendingAction = EUIPopupAction::None;

  private:
    static constexpr EUIPopupContentAlign ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Medium;
    static constexpr EUIPopupContentVerticalAlign ContentVerticalAlign =
        EUIPopupContentVerticalAlign::Center;
    static constexpr float MessageLineGap = DefaultContentLineGap;
};
