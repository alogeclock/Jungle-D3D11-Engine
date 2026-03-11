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

std::vector<std::string> FStageIntroPopup::SplitMessageLines(const std::string &InMessage) const
{
    std::vector<std::string> Result;

    if (InMessage.empty())
    {
        Result.push_back("");
        return Result;
    }

    std::size_t Start = 0;

    while (Start <= InMessage.size())
    {
        const std::size_t End = InMessage.find('\n', Start);

        if (End == std::string::npos)
        {
            Result.push_back(InMessage.substr(Start));
            break;
        }

        Result.push_back(InMessage.substr(Start, End - Start));
        Start = End + 1;
    }

    if (Result.empty())
    {
        Result.push_back("");
    }

    return Result;
}

int FStageIntroPopup::GetTotalPages() const
{
    if (Messages.empty())
    {
        return 1;
    }

    return static_cast<int>(Messages.size());
}

void FStageIntroPopup::GoToNextPage()
{
    const int LastPageIndex = GetTotalPages() - 1;

    if (CurrentPage < LastPageIndex)
    {
        ++CurrentPage;
    }
}

void FStageIntroPopup::ResetPage()
{
    const int LastPageIndex = GetTotalPages() - 1;

    if (CurrentPage < 0)
    {
        CurrentPage = 0;
    }

    if (CurrentPage > LastPageIndex)
    {
        CurrentPage = LastPageIndex;
    }
}

const std::string &FStageIntroPopup::GetCurrentPageMessage() const
{
    static const std::string EmptyMessage;

    if (Messages.empty())
    {
        return EmptyMessage;
    }

    if (CurrentPage < 0 || CurrentPage >= static_cast<int>(Messages.size()))
    {
        return EmptyMessage;
    }

    return Messages[CurrentPage];
}

void FStageIntroPopup::DrawBottomButtonArea(const FPopupFrameLayout &Layout)
{
    const bool bIsLastPage = (CurrentPage + 1) >= GetTotalPages();

    if (!bIsLastPage)
    {
        if (DrawBottomButton(Layout, "Next"))
        {
            GoToNextPage();
        }

        return;
    }

    if (DrawBottomButton(Layout, "OK, Start!"))
    {
        PendingAction = EUIPopupAction::StartStage;
    }
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

    ResetPage();

    const std::string &CurrentMessage = GetCurrentPageMessage();
    const std::vector<std::string> MessageLines = SplitMessageLines(CurrentMessage);

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

    DrawBottomButtonArea(Layout);

    ResetPage();

    if (PendingAction == EUIPopupAction::StartStage)
    {
        Close();
    }

    EndPopupWindow();
}
