#include "pch.h"
#include "ScoreboardPopup.h"
#include <cstdio>
#include <string>

namespace
{
    const char *GetRankSuffix(int Rank)
    {
        const int LastTwoDigits = Rank % 100;
        if (LastTwoDigits >= 11 && LastTwoDigits <= 13)
            return "th";

        const int LastDigit = Rank % 10;
        if (LastDigit == 1)
            return "st";
        if (LastDigit == 2)
            return "nd";
        if (LastDigit == 3)
            return "rd";

        return "th";
    }
}

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

    const float StartY = Layout.ContentTop + TableTopOffset;
    const float HeaderY = StartY;
    const float RowStartY = HeaderY + LineHeight + HeaderToRowsGap;

    const int PageStartIndex = GetPageStartIndex();
    const int PageEntryCount = GetPageEntryCount();

    int LeftCount = PageEntryCount;
    if (LeftCount > MaxRowsPerColumn)
        LeftCount = MaxRowsPerColumn;

    int RightCount = PageEntryCount - LeftCount;
    if (RightCount < 0)
        RightCount = 0;

    const float MiddleLineX = Layout.ContentLeft + Layout.ContentWidth * 0.5f;
    const float DividerTopY = HeaderY - ColumnDividerPadding;
    const float DividerBottomY =
        RowStartY + static_cast<float>(MaxRowsPerColumn) * LineHeight +
        static_cast<float>(MaxRowsPerColumn - 1) * RowGap + ColumnDividerPadding;

    const ImVec2 WindowPos = ImGui::GetWindowPos();
    ImDrawList *DrawList = ImGui::GetWindowDrawList();

    DrawList->AddLine(ImVec2(WindowPos.x + MiddleLineX, WindowPos.y + DividerTopY),
                      ImVec2(WindowPos.x + MiddleLineX, WindowPos.y + DividerBottomY),
                      ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);

    DrawColumnHeader(LeftColumnX, HeaderY);
    DrawColumnHeader(RightColumnX, HeaderY);

    const float HeaderLineY = HeaderY + LineHeight + HeaderLineOffset;
    DrawList->AddLine(ImVec2(WindowPos.x + LeftColumnX, WindowPos.y + HeaderLineY),
                      ImVec2(WindowPos.x + LeftColumnX + ColumnWidth, WindowPos.y + HeaderLineY),
                      ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);

    DrawList->AddLine(ImVec2(WindowPos.x + RightColumnX, WindowPos.y + HeaderLineY),
                      ImVec2(WindowPos.x + RightColumnX + ColumnWidth, WindowPos.y + HeaderLineY),
                      ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);

    DrawColumnBlock(Layout, LeftColumnX, PageStartIndex, LeftCount);
    DrawColumnBlock(Layout, RightColumnX, PageStartIndex + LeftCount, RightCount);

    ImGui::SetWindowFontScale(1.0f);
}

void FScoreboardPopup::DrawColumnBlock(const FPopupFrameLayout &Layout, float BaseX,
                                       int StartEntryIndex, int EntryCount)
{
    if (EntryCount <= 0)
        return;

    const float LineHeight = ImGui::GetTextLineHeight();
    const float HeaderY = Layout.ContentTop + TableTopOffset;
    const float RowStartY = HeaderY + LineHeight + HeaderToRowsGap;

    for (int i = 0; i < EntryCount; ++i)
    {
        const float Y = RowStartY + static_cast<float>(i) * (LineHeight + RowGap);
        const int EntryIndex = StartEntryIndex + i;
        const int Rank = EntryIndex + 1;

        DrawRow(BaseX, Y, Rank, Entries[EntryIndex]);
    }
}

void FScoreboardPopup::DrawColumnHeader(float BaseX, float Y)
{
    const float RankX = BaseX;
    const float NameX = BaseX + RankColumnWidth;
    const float ScoreX = BaseX + RankColumnWidth + NameColumnWidth;
    const float StageX = BaseX + RankColumnWidth + NameColumnWidth + ScoreColumnWidth;

    ImGui::SetCursorPos(ImVec2(RankX, Y));
    ImGui::TextUnformatted("RANK");

    ImGui::SetCursorPos(ImVec2(NameX, Y));
    ImGui::TextUnformatted("NAME");

    ImGui::SetCursorPos(ImVec2(ScoreX, Y));
    ImGui::TextUnformatted("SCORE");

    ImGui::SetCursorPos(ImVec2(StageX, Y));
    ImGui::TextUnformatted("STAGE");
}

void FScoreboardPopup::DrawRow(float BaseX, float Y, int Rank, const FScoreRecord &Entry)
{
    char RankBuffer[16]{};
    char NameBuffer[16]{};
    char ScoreBuffer[16]{};
    char StageBuffer[16]{};

    std::snprintf(RankBuffer, sizeof(RankBuffer), "%d%s", Rank, GetRankSuffix(Rank));
    std::snprintf(NameBuffer, sizeof(NameBuffer), "%-6.6s", Entry.Nickname.c_str());
    std::snprintf(ScoreBuffer, sizeof(ScoreBuffer), "%04d", Entry.Score);
    std::snprintf(StageBuffer, sizeof(StageBuffer), "S-%d", Entry.Stage);

    const float RankX = BaseX;
    const float NameX = BaseX + RankColumnWidth;
    const float ScoreX = BaseX + RankColumnWidth + NameColumnWidth;
    const float StageX = BaseX + RankColumnWidth + NameColumnWidth + ScoreColumnWidth;

    ImGui::SetCursorPos(ImVec2(RankX, Y));
    ImGui::TextUnformatted(RankBuffer);

    ImGui::SetCursorPos(ImVec2(NameX, Y));
    ImGui::TextUnformatted(NameBuffer);

    ImGui::SetCursorPos(ImVec2(ScoreX, Y));
    ImGui::TextUnformatted(ScoreBuffer);

    ImGui::SetCursorPos(ImVec2(StageX, Y));
    ImGui::TextUnformatted(StageBuffer);
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