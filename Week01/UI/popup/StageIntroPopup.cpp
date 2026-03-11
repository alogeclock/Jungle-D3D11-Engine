#include "pch.h"
#include "StageIntroPopup.h"
#include <cstdio>
#include <vector>

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

std::vector<std::string> FStageIntroPopup::SplitMessageLines() const
{
    std::vector<std::string> Result;

    if (Message.empty())
    {
        Result.push_back("");
        return Result;
    }

    std::size_t Start = 0;

    while (Start <= Message.size())
    {
        const std::size_t End = Message.find('\n', Start);

        if (End == std::string::npos)
        {
            Result.push_back(Message.substr(Start));
            break;
        }

        Result.push_back(Message.substr(Start, End - Start));
        Start = End + 1;
    }

    if (Result.empty())
    {
        Result.push_back("");
    }

    return Result;
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

    const std::vector<std::string> MessageLines = SplitMessageLines();

    std::vector<const char *> LinePtrs;
    LinePtrs.reserve(MessageLines.size());

    for (const std::string &Line : MessageLines)
    {
        LinePtrs.push_back(Line.c_str());
    }

    DrawTextBlock(Layout,
                  LinePtrs.data(),
                  static_cast<int>(LinePtrs.size()),
                  MessageLineGap,
                  ContentAlign,
                  ContentTextSize,
                  ContentVerticalAlign);

    if (DrawBottomButton(Layout, "OK, Start!"))
    {
        PendingAction = EUIPopupAction::StartStage;
        Close();
    }

    EndPopupWindow();
}