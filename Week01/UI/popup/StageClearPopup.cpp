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
            PendingAction = EUIPopupAction::GoToTitleScene;
        }
    }
    else
    {
        DrawNormalClearMessage(Layout);

        if (DrawBottomButton(Layout, "Next Stage", 0, 2))
        {
            PendingAction = EUIPopupAction::GoToNextStage;
        }

        if (DrawBottomButton(Layout, "Go To Title", 1, 2))
        {
            PendingAction = EUIPopupAction::GoToTitleScene;
        }
    }

    EndPopupWindow();
}

void FStageClearPopup::DrawAllClearedMessage(const FPopupFrameLayout &Layout)
{
    const char *Line1 = "You cleared all stages!";
    const char *Line2 = "Would you like to save your score?";

    ImGui::SetWindowFontScale(GetContentFontScale(ContentTextSize));

    const ImVec2 Size1 = ImGui::CalcTextSize(Line1);
    const ImVec2 Size2 = ImGui::CalcTextSize(Line2);

    const float BlockHeight = Size1.y + MessageLineGap + Size2.y;
    const float StartY = Layout.ContentTop + (Layout.ContentHeight - BlockHeight) * 0.5f;

    const float X1 = GetAlignedX(Layout, Size1.x, ContentAlign);
    const float X2 = GetAlignedX(Layout, Size2.x, ContentAlign);

    ImGui::SetCursorPos(ImVec2(X1, StartY));
    ImGui::TextUnformatted(Line1);

    ImGui::SetCursorPos(ImVec2(X2, StartY + Size1.y + MessageLineGap));
    ImGui::TextUnformatted(Line2);

    ImGui::SetWindowFontScale(1.0f);
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