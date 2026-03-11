#include "pch.h"
#include "EndingPopup.h"

#include <algorithm>
#include <vector>
#include "Core/GameContext.h"

namespace
{
    float GetAlignedContentStartY(const FPopupFrameLayout &Layout, float ContentHeight,
                                  EUIPopupContentVerticalAlign Align)
    {
        if (Align == EUIPopupContentVerticalAlign::Top)
        {
            return Layout.ContentTop;
        }

        return Layout.ContentTop + (Layout.ContentHeight - ContentHeight) * 0.5f;
    }
} // namespace

EUIPopupAction FEndingPopup::ConsumeAction()
{
    const EUIPopupAction Result = PendingAction;
    PendingAction = EUIPopupAction::None;
    return Result;
}

void FEndingPopup::Update(FGameContext &Context) {}

int FEndingPopup::GetTotalPages() const
{
    if (static_cast<int>(Messages.size()) < 1)
        return 1;
    else
        return static_cast<int>(Messages.size());
}

void FEndingPopup::ResetPage()
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

const std::string &FEndingPopup::GetCurrentPageMessage() const
{
    static const std::string EmptyMessage;

    if (Messages.empty())
    {
        return EmptyMessage;
    }

    return Messages[CurrentPage];
}

std::vector<std::string> FEndingPopup::SplitMessageLines(const std::string &InMessage) const
{
    std::vector<std::string> Lines;
    std::string              CurrentLine;

    for (char Ch : InMessage)
    {
        if (Ch == '\n')
        {
            Lines.push_back(CurrentLine);
            CurrentLine.clear();
        }
        else
        {
            CurrentLine.push_back(Ch);
        }
    }

    Lines.push_back(CurrentLine);

    if (Lines.empty())
    {
        Lines.push_back("");
    }

    return Lines;
}

void FEndingPopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("EndingPopup", "Ending", ImVec2(DefaultPopupWidth, DefaultPopupHeight),
                          Layout))
    {
        return;
    }

    if (bShowSaveConfirm)
    {
        DrawSaveConfirmContent(Layout);
    }
    else
    {
        DrawMessageContent(Layout);
    }

    EndPopupWindow();
}

void FEndingPopup::DrawMessageContent(const FPopupFrameLayout &Layout)
{
    const std::vector<std::string> Lines = SplitMessageLines(GetCurrentPageMessage());

    ImGui::SetWindowFontScale(GetContentFontScale(ContentTextSize));

    std::vector<ImVec2> LineSizes;
    LineSizes.reserve(Lines.size());

    float TotalTextHeight = 0.0f;
    for (const std::string &Line : Lines)
    {
        const ImVec2 Size = ImGui::CalcTextSize(Line.c_str());
        LineSizes.push_back(Size);
        TotalTextHeight += Size.y;
    }

    if (!Lines.empty())
    {
        TotalTextHeight += MessageLineGap * static_cast<float>(Lines.size() - 1);
    }

    const float StartY = GetAlignedContentStartY(Layout, TotalTextHeight, ContentVerticalAlign);

    float CurrentY = StartY;
    for (size_t i = 0; i < Lines.size(); ++i)
    {
        const float X = GetAlignedX(Layout, LineSizes[i].x, ContentAlign);
        ImGui::SetCursorPos(ImVec2(X, CurrentY));
        ImGui::TextUnformatted(Lines[i].c_str());
        CurrentY += LineSizes[i].y + MessageLineGap;
    }

    ImGui::SetWindowFontScale(1.0f);

    if (DrawBottomButton(Layout, "Next"))
    {
        const int LastPageIndex = GetTotalPages() - 1;

        if (CurrentPage < LastPageIndex)
        {
            ++CurrentPage;
        }
        else
        {
            bShowSaveConfirm = true;
        }
    }
}

void FEndingPopup::DrawSaveConfirmContent(const FPopupFrameLayout &Layout)
{
    const char *Question = "점수를 저장하시겠습니까?";

    ImGui::SetWindowFontScale(GetContentFontScale(ContentTextSize));

    const ImVec2 TextSize = ImGui::CalcTextSize(Question);
    const float  TextX = GetAlignedX(Layout, TextSize.x, ContentAlign);
    const float  TextY = GetAlignedContentStartY(Layout, TextSize.y, ContentVerticalAlign);

    ImGui::SetCursorPos(ImVec2(TextX, TextY));
    ImGui::TextUnformatted(Question);

    ImGui::SetWindowFontScale(1.0f);

    if (DrawBottomButton(Layout, "예", 0, 2))
    {
        PendingAction = EUIPopupAction::OpenSaveScorePopup;
    }

    if (DrawBottomButton(Layout, "아니오", 1, 2))
    {
        PendingAction = EUIPopupAction::OpenGoToTitlePopup;
    }
}