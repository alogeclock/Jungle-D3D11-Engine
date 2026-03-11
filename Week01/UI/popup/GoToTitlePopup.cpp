#include "pch.h"
#include "GoToTitlePopup.h"

EUIPopupAction FGoToTitlePopup::ConsumeAction()
{
    EUIPopupAction Result = PendingAction;
    PendingAction = EUIPopupAction::None;
    return Result;
}

void FGoToTitlePopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("GoToTitle", "Notice", ImVec2(DefaultPopupWidth, DefaultPopupHeight),
                          Layout))
    {
        return;
    }

    DrawTextBlock(Layout, {"Go to the title."}, 0.0f, EUIPopupContentAlign::Center,
                  EUIPopupContentTextSize::Big, EUIPopupContentVerticalAlign::Center);

    if (DrawBottomButton(Layout, "OK"))
    {
        PendingAction = EUIPopupAction::GoToTitleScene;
    }

    EndPopupWindow();
}
