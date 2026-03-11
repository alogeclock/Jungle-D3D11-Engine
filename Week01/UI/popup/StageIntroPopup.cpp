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
        return 1;

    const int TotalPages = static_cast<int>(Messages.size());
    return TotalPages > 0 ? TotalPages : 1;
}

void FStageIntroPopup::GoToNextPage()
{
    const int TotalPages = GetTotalPages();
    const int LastPageIndex = TotalPages - 1;

    if (CurrentPage < LastPageIndex)
    {
        ++CurrentPage;
    }
}

void FStageIntroPopup::ResetPage()
{
    const int TotalPages = GetTotalPages();
    const int LastPageIndex = TotalPages - 1;

    if (CurrentPage < 0)
        CurrentPage = 0;

    if (CurrentPage > LastPageIndex)
        CurrentPage = LastPageIndex;
}

const std::string &FStageIntroPopup::GetCurrentPageMessage() const
{
    static const std::string EmptyMessage;

    if (Messages.empty())
        return EmptyMessage;

    if (CurrentPage < 0 || CurrentPage >= static_cast<int>(Messages.size()))
        return EmptyMessage;

    return Messages[CurrentPage];
}

void FStageIntroPopup::DrawBottomButtons(const FPopupFrameLayout &Layout,
                                         bool bHasPrevPage, bool bHasNextPage)
{
    if (bHasNextPage)
    {
        if (bHasPrevPage)
        {
            if (DrawBottomButton(Layout, "Prev", 0, 2))
            {
                --CurrentPage;
            }

            if (DrawBottomButton(Layout, "Next", 1, 2))
            {
                ++CurrentPage;
            }
        }
        else
        {
            if (DrawBottomButton(Layout, "Next", 0, 1))
            {
                ++CurrentPage;
            }
        }

        return;
    }

    if (bHasPrevPage)
    {
        if (DrawBottomButton(Layout, "Prev", 0, 2))
        {
            --CurrentPage;
        }

        if (DrawBottomButton(Layout, "OK, Start!", 1, 2))
        {
            PendingAction = EUIPopupAction::StartStage;
        }
    }
    else
    {
        if (DrawBottomButton(Layout, "OK, Start!", 0, 1))
        {
            PendingAction = EUIPopupAction::StartStage;
        }
    }
}

void FStageIntroPopup::DrawPageText(const FPopupFrameLayout &Layout)
{
    ImGui::SetWindowFontScale(GetContentFontScale(EUIPopupContentTextSize::Small));

    char PageBuffer[32]{};
    std::snprintf(PageBuffer, sizeof(PageBuffer), "Page %d / %d", CurrentPage + 1, GetTotalPages());

    const ImVec2 PageSize = ImGui::CalcTextSize(PageBuffer);
    const float X = GetAlignedX(Layout, PageSize.x, EUIPopupContentAlign::Center);
    const float Y = Layout.ContentBottom - PageSize.y - 4.0f;

    ImGui::SetCursorPos(ImVec2(X, Y));
    ImGui::TextUnformatted(PageBuffer);

    ImGui::SetWindowFontScale(1.0f);
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

    DrawPageText(Layout);

    const int TotalPages = GetTotalPages();
    const bool bHasPrevPage = CurrentPage > 0;
    const bool bHasNextPage = (CurrentPage + 1) < TotalPages;

    DrawBottomButtons(Layout, bHasPrevPage, bHasNextPage);

    ResetPage();

    if (PendingAction == EUIPopupAction::StartStage)
    {
        Close();
    }

    EndPopupWindow();
}
