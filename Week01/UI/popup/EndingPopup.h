#pragma once

#include <string>
#include <vector>
#include "UIPopupAction.h"
#include "UIPopupBase.h"

struct FGameContext;

class FEndingPopup : public FUIPopupBase
{
  public:
    void SetData(const std::string &InTitle, const std::vector<std::string> &InMessages)
    {
        Title = InTitle;
        Messages = InMessages;
        CurrentPage = 0;
        ResetPage();
    }

    void SetTitle(const std::string &InTitle)
    {
        Title = InTitle;
    }

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
    std::vector<std::string> SplitMessageLines(const std::string &InMessage) const;

    int                GetTotalPages() const;
    void               GoToNextPage();
    void               ResetPage();
    const std::string &GetCurrentPageMessage() const;
    void               DrawBottomButtonArea(const FPopupFrameLayout &Layout);

  private:
    std::string              Title = "Ending";
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
