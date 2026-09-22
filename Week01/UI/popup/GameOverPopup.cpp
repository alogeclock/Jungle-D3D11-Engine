#include "pch.h"
#include "GameOverPopup.h"

EUIPopupAction FGameOverPopup::ConsumeAction()
{
    EUIPopupAction Result = PendingAction;
    PendingAction = EUIPopupAction::None;
    return Result;
}

void FGameOverPopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("GameOver", "Game Over",
                          ImVec2(DefaultPopupWidth, DefaultPopupHeight), Layout))
    {
        return;
    }

    const char *Message = "You failed the stage.";

    ImGui::SetWindowFontScale(GetContentFontScale(ContentTextSize));

    const ImVec2 TextSize = CalcUtf8TextSize(Message);
    const float X = GetAlignedX(Layout, TextSize.x, ContentAlign);
    const float Y = Layout.ContentTop + (Layout.ContentHeight - TextSize.y) * 0.5f;

    ImGui::SetCursorPos(ImVec2(X, Y));
    TextUnformattedUtf8(Message);

    ImGui::SetWindowFontScale(1.0f);

    if (DrawBottomButton(Layout, "Save Score", 0, 2))
    {
        PendingAction = EUIPopupAction::OpenSaveScorePopup;
    }

    if (DrawBottomButton(Layout, "Go To Title", 1, 2))
    {
        PendingAction = EUIPopupAction::OpenGoToTitlePopup;
    }

    EndPopupWindow();
}
