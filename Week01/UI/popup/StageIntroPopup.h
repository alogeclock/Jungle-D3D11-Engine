#pragma once

#include <string>
#include <vector>
#include "UIPopupBase.h"

struct FGameContext;

class FStageIntroPopup : public FUIPopupBase
{
  public:
    void SetData(int InStageNumber, const std::vector<std::string> &InMessages)
    {
        StageNumber = InStageNumber;
        Messages = InMessages;
        CurrentPage = 0;
        ResetPage();
    }

    void SetStageNumber(int InStageNumber) { StageNumber = InStageNumber; }

    void SetMessages(const std::vector<std::string> &InMessages)
    {
        Messages = InMessages;
        CurrentPage = 0;
        ResetPage();
    }

    EUIPopupAction ConsumeAction();

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    std::string              GetPopupTitle() const;
    std::vector<std::string> SplitMessageLines(const std::string &InMessage) const;

    int                GetTotalPages() const;
    void               GoToNextPage();
    void               ResetPage();
    const std::string &GetCurrentPageMessage() const;
    void               DrawBottomButtonArea(const FPopupFrameLayout &Layout);

  private:
    int                      StageNumber = 1;
    std::vector<std::string> Messages;
    int                      CurrentPage = 0;
    EUIPopupAction           PendingAction = EUIPopupAction::None;

  private:
    static constexpr EUIPopupContentAlign ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Medium;
    static constexpr EUIPopupContentVerticalAlign ContentVerticalAlign =
        EUIPopupContentVerticalAlign::Center;
    static constexpr float MessageLineGap = DefaultContentLineGap;
};