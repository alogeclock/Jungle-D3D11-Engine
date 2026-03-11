#include "pch.h"
#include "ScoreboardPopup.h"
#include <cstdio>

void FScoreboardPopup::SetEntries(const std::vector<FScoreRecord> &InEntries)
{
    Entries = InEntries;
    CurrentPage = 0;
    ResetPage();
}

void FScoreboardPopup::GoToNextPage()
{
    const int TotalPages = GetTotalPages();
    const int LastPageIndex = TotalPages - 1;

    if (CurrentPage < LastPageIndex)
    {
        ++CurrentPage;
    }
}

void FScoreboardPopup::ResetPage()
{
    const int TotalPages = GetTotalPages();
    const int LastPageIndex = TotalPages - 1;

    if (CurrentPage < 0)
        CurrentPage = 0;

    if (CurrentPage > LastPageIndex)
        CurrentPage = LastPageIndex;
}

EUIPopupAction FScoreboardPopup::ConsumeAction()
{
    EUIPopupAction Result = PendingAction;
    PendingAction = EUIPopupAction::None;
    return Result;
}

int FScoreboardPopup::GetTotalPages() const
{
    if (Entries.empty())
        return 1;

    const int Count = static_cast<int>(Entries.size());
    int TotalPages = Count / EntriesPerPage;
    if ((Count % EntriesPerPage) != 0)
        ++TotalPages;

    if (TotalPages <= 0)
        TotalPages = 1;

    return TotalPages;
}

int FScoreboardPopup::GetPageStartIndex() const
{
    return CurrentPage * EntriesPerPage;
}

int FScoreboardPopup::GetPageEntryCount() const
{
    const int StartIndex = GetPageStartIndex();
    const int TotalCount = static_cast<int>(Entries.size());

    if (StartIndex >= TotalCount)
        return 0;

    const int Remaining = TotalCount - StartIndex;
    return (Remaining < EntriesPerPage) ? Remaining : EntriesPerPage;
}

void FScoreboardPopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("Scoreboard", "Scoreboard",
                          ImVec2(DefaultPopupWidth, DefaultPopupHeight), Layout))
    {
        return;
    }

    ResetPage();
    DrawEntries(Layout);
    DrawPageText(Layout);

    const int TotalPages = GetTotalPages();
    const bool bHasPrevPage = CurrentPage > 0;
    const bool bHasNextPage = (CurrentPage + 1) < TotalPages;

    DrawBottomButtons(Layout, bHasPrevPage, bHasNextPage);

    ResetPage();
    EndPopupWindow();
}

void FScoreboardPopup::DrawBottomButtons(const FPopupFrameLayout &Layout,
                                         bool bHasPrevPage, bool bHasNextPage)
{
    if (bHasPrevPage && bHasNextPage)
    {
        if (DrawBottomButton(Layout, "Prev", 0, 3))
        {
            --CurrentPage;
        }

        if (DrawBottomButton(Layout, "Next", 1, 3))
        {
            ++CurrentPage;
        }

        if (DrawBottomButton(Layout, "Close", 2, 3))
        {
            PendingAction = EUIPopupAction::ClosePopup;
        }
    }
    else if (bHasPrevPage)
    {
        if (DrawBottomButton(Layout, "Prev", 0, 2))
        {
            --CurrentPage;
        }

        if (DrawBottomButton(Layout, "Close", 1, 2))
        {
            PendingAction = EUIPopupAction::ClosePopup;
        }
    }
    else if (bHasNextPage)
    {
        if (DrawBottomButton(Layout, "Next", 0, 2))
        {
            ++CurrentPage;
        }

        if (DrawBottomButton(Layout, "Close", 1, 2))
        {
            PendingAction = EUIPopupAction::ClosePopup;
        }
    }
    else
    {
        if (DrawBottomButton(Layout, "Close", 0, 1))
        {
            PendingAction = EUIPopupAction::ClosePopup;
        }
    }
}

void FScoreboardPopup::DrawEntries(const FPopupFrameLayout &Layout)
{
    ImGui::SetWindowFontScale(GetContentFontScale(ContentTextSize));

    const float LineHeight = ImGui::GetTextLineHeight();
    const float ColumnWidth = (Layout.ContentWidth - ColumnGap) * 0.5f;

    const float LeftColumnX = Layout.ContentLeft;
    const float RightColumnX = LeftColumnX + ColumnWidth + ColumnGap;

    const float StartY = Layout.ContentTop + 8.0f;
    const int PageStartIndex = GetPageStartIndex();
    const int PageEntryCount = GetPageEntryCount();

    const float MiddleLineX = Layout.ContentLeft + Layout.ContentWidth * 0.5f;
    const float TopY = StartY - 2.0f;
    const float BottomY = StartY + static_cast<float>(MaxRowsPerColumn) * LineHeight +
                          static_cast<float>(MaxRowsPerColumn - 1) * RowGap + 2.0f;

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const ImVec2 WindowPos = ImGui::GetWindowPos();

    DrawList->AddLine(ImVec2(WindowPos.x + MiddleLineX, WindowPos.y + TopY),
                      ImVec2(WindowPos.x + MiddleLineX, WindowPos.y + BottomY),
                      ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);

    char RankBuffer[16]{};
    char NameBuffer[16]{};
    char StageBuffer[32]{};
    char ScoreBuffer[16]{};

    for (int i = 0; i < PageEntryCount; ++i)
    {
        const int EntryIndex = PageStartIndex + i;
        const int ColumnIndex = i / MaxRowsPerColumn;
        const int RowIndex = i % MaxRowsPerColumn;

        float BaseX = LeftColumnX;
        if (ColumnIndex == 1)
            BaseX = RightColumnX;

        const float Y = StartY + static_cast<float>(RowIndex) * (LineHeight + RowGap);

        std::snprintf(RankBuffer, sizeof(RankBuffer), "%2d.", EntryIndex + 1);
        std::snprintf(NameBuffer, sizeof(NameBuffer), "%-6.6s", Entries[EntryIndex].Nickname.c_str());
        std::snprintf(StageBuffer, sizeof(StageBuffer), "Stage %-2d", Entries[EntryIndex].Stage);
        std::snprintf(ScoreBuffer, sizeof(ScoreBuffer), "%6d", Entries[EntryIndex].Score);

        const float RankX = BaseX;
        const float NameX = BaseX + RankColumnWidth;
        const float StageX = BaseX + RankColumnWidth + NameColumnWidth;
        const float ScoreX = BaseX + RankColumnWidth + NameColumnWidth + StageColumnWidth;

        ImGui::SetCursorPos(ImVec2(RankX, Y));
        ImGui::TextUnformatted(RankBuffer);

        ImGui::SetCursorPos(ImVec2(NameX, Y));
        ImGui::TextUnformatted(NameBuffer);

        ImGui::SetCursorPos(ImVec2(StageX, Y));
        ImGui::TextUnformatted(StageBuffer);

        ImGui::SetCursorPos(ImVec2(ScoreX, Y));
        ImGui::TextUnformatted(ScoreBuffer);
    }

    ImGui::SetWindowFontScale(1.0f);
}

void FScoreboardPopup::DrawPageText(const FPopupFrameLayout &Layout)
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