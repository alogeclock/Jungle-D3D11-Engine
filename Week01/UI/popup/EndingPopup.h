#pragma once

#include <string>
#include <vector>
#include "UIPopupBase.h"

struct FGameContext;

class FEndingPopup : public FUIPopupBase
{
  public:
    void SetMessages(const std::vector<std::string> &InMessages)
    {
        Messages = InMessages;
        CurrentPage = 0;
        bShowSaveConfirm = false;
        ResetPage();
    }

    EUIPopupAction ConsumeAction();

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    std::vector<std::string> SplitMessageLines(const std::string &InMessage) const;

    int                GetTotalPages() const;
    void               ResetPage();
    const std::string &GetCurrentPageMessage() const;

    void DrawMessageContent(const FPopupFrameLayout &Layout);
    void DrawSaveConfirmContent(const FPopupFrameLayout &Layout);

  private:
    std::vector<std::string> Messages;
    int                      CurrentPage = 0;
    bool                     bShowSaveConfirm = false;
    EUIPopupAction           PendingAction = EUIPopupAction::None;

  private:
    static constexpr EUIPopupContentAlign ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Medium;
    static constexpr EUIPopupContentVerticalAlign ContentVerticalAlign =
        EUIPopupContentVerticalAlign::Center;
    static constexpr float MessageLineGap = DefaultContentLineGap;
};