#include "pch.h"
#include "StageIntroPopup.h"
#include <cstdio>

EUIPopupAction FStageIntroPopup::ConsumeAction()
{
    const EUIPopupAction Result = PendingAction;
    PendingAction = EUIPopupAction::None;
    return Result;
}

void FStageIntroPopup::Update(FGameContext &Context)
{
}

std::string FStageIntroPopup::GetPopupTitle() const
{
    char Buffer[64]{};
    std::snprintf(Buffer, sizeof(Buffer), "Stage %d", StageNumber);
    return std::string(Buffer);
}

void FStageIntroPopup::Render(FGameContext &Context)
{
    const std::string TitleText = GetPopupTitle();

    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("StageIntroPopup", TitleText.c_str(),
                          ImVec2(DefaultPopupWidth, DefaultPopupHeight), Layout))
    {
        return;
    }

    const char *Lines[] = {
        Message.c_str(),
    };

    DrawTextBlock(Layout, Lines, 1, MessageLineGap, ContentAlign, ContentTextSize,
                  ContentVerticalAlign);

    if (DrawBottomButton(Layout, "OK, Start!"))
    {
        PendingAction = EUIPopupAction::StartStage;
        Close();
    }

    EndPopupWindow();
}
