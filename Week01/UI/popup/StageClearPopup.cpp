#include "pch.h"
#include "StageClearPopup.h"
#include <cstdio>

EUIPopupAction FStageClearPopup::ConsumeAction()
{
    EUIPopupAction Result = PendingAction;
    PendingAction = EUIPopupAction::None;
    return Result;
}

void FStageClearPopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("StageClear", "Stage Clear",
                          ImVec2(DefaultPopupWidth, DefaultPopupHeight), Layout))
    {
        return;
    }

    if (bAllCleared)
    {
        DrawAllClearedMessage(Layout);

        if (DrawBottomButton(Layout, "Yes", 0, 2))
        {
            PendingAction = EUIPopupAction::OpenSaveScorePopup;
        }

        if (DrawBottomButton(Layout, "No", 1, 2))
        {
            PendingAction = EUIPopupAction::OpenGoToTitlePopup;
        }
    }
    else
    {
        DrawNormalClearMessage(Layout);

        if (DrawBottomButton(Layout, "Next Stage", 0, 1))
        {
            PendingAction = EUIPopupAction::GoToNextStage;
        }
    }

    EndPopupWindow();
}

void FStageClearPopup::DrawAllClearedMessage(const FPopupFrameLayout &Layout)
{
    DrawTextBlock(
        Layout,
        {
            "You cleared all stages!",
            "Would you like to save your score?"
        },
        MessageLineGap,
        ContentAlign,
        ContentTextSize,
        EUIPopupContentVerticalAlign::Center);
}

void FStageClearPopup::DrawNormalClearMessage(const FPopupFrameLayout &Layout)
{
    char Message[128]{};
    std::snprintf(Message, sizeof(Message), "Stage %d cleared!", ClearedStage);

    ImGui::SetWindowFontScale(GetContentFontScale(ContentTextSize));

    const ImVec2 TextSize = ImGui::CalcTextSize(Message);
    const float X = GetAlignedX(Layout, TextSize.x, ContentAlign);
    const float Y = Layout.ContentTop + (Layout.ContentHeight - TextSize.y) * 0.5f;

    ImGui::SetCursorPos(ImVec2(X, Y));
    ImGui::TextUnformatted(Message);

    ImGui::SetWindowFontScale(1.0f);
}