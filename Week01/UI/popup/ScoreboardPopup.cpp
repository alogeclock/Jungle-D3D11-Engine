#include "pch.h"
#include <algorithm>
#include <cstdio>
#include "ScoreboardPopup.h"
#include "imgui/imgui.h"

void FScoreboardPopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("Scoreboard", "SCORE BOARD",
                          ImVec2(DefaultPopupWidth, DefaultPopupHeight), Layout))
        return;

    if (Records.empty())
        DrawEmpty(Layout);
    else
        DrawRecords(Layout);

    if (DrawBottomButton(Layout, "Close"))
    {
        ImGui::CloseCurrentPopup();
        Close();
    }

    EndPopupWindow();
}

void FScoreboardPopup::DrawEmpty(const FPopupFrameLayout &Layout)
{
    const float ContentFontScale = GetContentFontScale(ContentTextSize);
    ImGui::SetWindowFontScale(ContentFontScale);

    const char  *Text = "NO SCORES YET";
    const ImVec2 TextSize = ImGui::CalcTextSize(Text);
    const float  X = GetAlignedX(Layout, TextSize.x, ContentAlign);
    const float  Y = Layout.ContentTop + 4.0f;

    ImGui::SetCursorPos(ImVec2(X, Y));
    ImGui::TextUnformatted(Text);

    ImGui::SetWindowFontScale(1.0f);
}

void FScoreboardPopup::DrawRecords(const FPopupFrameLayout &Layout)
{
    const float ContentFontScale = GetContentFontScale(ContentTextSize);
    ImGui::SetWindowFontScale(ContentFontScale);

    std::vector<FScoreRecord> LeftRecords;
    std::vector<FScoreRecord> RightRecords;

    LeftRecords.reserve(Records.size() < MaxRowsPerColumn ? Records.size() : MaxRowsPerColumn);
    RightRecords.reserve(Records.size() > MaxRowsPerColumn ? Records.size() - MaxRowsPerColumn : 0);

    for (std::size_t Index = 0; Index < Records.size(); ++Index)
    {
        if (Index < MaxRowsPerColumn)
            LeftRecords.push_back(Records[Index]);
        else
            RightRecords.push_back(Records[Index]);
    }

    const float CenterX = Layout.ContentLeft + Layout.ContentWidth * 0.5f;
    const float HalfWidth = (Layout.ContentWidth - SectionGap) * 0.5f;

    const float LeftAreaCenterX = Layout.ContentLeft + HalfWidth * 0.5f;
    const float RightAreaLeft = CenterX + SectionGap * 0.5f;
    const float RightAreaCenterX = RightAreaLeft + HalfWidth * 0.5f;

    const float StartY = Layout.ContentTop + 4.0f;

    const float DividerTop = Layout.ContentTop + DividerMarginY;
    const float DividerBottom = Layout.ContentTop + Layout.ContentHeight - DividerMarginY;

    ImDrawList  *DrawList = ImGui::GetWindowDrawList();
    const ImVec2 WindowPos = ImGui::GetWindowPos();

    DrawList->AddLine(ImVec2(WindowPos.x + CenterX, WindowPos.y + DividerTop),
                      ImVec2(WindowPos.x + CenterX, WindowPos.y + DividerBottom),
                      IM_COL32(150, 150, 150, 255), DividerThickness);

    DrawRecordColumn(ImVec2(LeftAreaCenterX, StartY), LeftRecords);
    DrawRecordColumn(ImVec2(RightAreaCenterX, StartY), RightRecords);

    ImGui::SetWindowFontScale(1.0f);
}

void FScoreboardPopup::DrawRecordColumn(const ImVec2                    &StartPos,
                                        const std::vector<FScoreRecord> &ColumnRecords)
{
    if (ColumnRecords.empty())
        return;

    char Buffer[64]{};

    const float FixedNameWidth = ImGui::CalcTextSize("AAAAAA").x;

    float MaxStageWidth = 0.0f;
    float MaxScoreWidth = 0.0f;

    for (const FScoreRecord &Record : ColumnRecords)
    {
        std::snprintf(Buffer, sizeof(Buffer), "STAGE %d", Record.Stage);
        const ImVec2 StageSize = ImGui::CalcTextSize(Buffer);

        std::snprintf(Buffer, sizeof(Buffer), "%d", Record.Score);
        const ImVec2 ScoreSize = ImGui::CalcTextSize(Buffer);

        if (StageSize.x > MaxStageWidth)
            MaxStageWidth = StageSize.x;
        if (ScoreSize.x > MaxScoreWidth)
            MaxScoreWidth = ScoreSize.x;
    }

    const float BlockWidth = FixedNameWidth + ColumnGap + MaxStageWidth + ColumnGap + MaxScoreWidth;
    const float StartX = StartPos.x - BlockWidth * 0.5f;
    const float StageX = StartX + FixedNameWidth + ColumnGap;
    const float ScoreX = StageX + MaxStageWidth + ColumnGap;

    const float LineHeight = ImGui::GetTextLineHeight();

    for (std::size_t Index = 0; Index < ColumnRecords.size(); ++Index)
    {
        const FScoreRecord &Record = ColumnRecords[Index];
        const float         RowY = StartPos.y + Index * (LineHeight + RowGap);

        std::string DisplayName = Record.Nickname;
        if (DisplayName.size() > 6)
            DisplayName = DisplayName.substr(0, 6);

        ImGui::SetCursorPos(ImVec2(StartX, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::TextUnformatted(DisplayName.c_str());
        ImGui::PopStyleColor();

        std::snprintf(Buffer, sizeof(Buffer), "STAGE %d", Record.Stage);
        ImGui::SetCursorPos(ImVec2(StageX, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.82f, 1.0f, 1.0f));
        ImGui::TextUnformatted(Buffer);
        ImGui::PopStyleColor();

        std::snprintf(Buffer, sizeof(Buffer), "%d", Record.Score);
        const ImVec2 ScoreSize = ImGui::CalcTextSize(Buffer);
        ImGui::SetCursorPos(ImVec2(ScoreX + MaxScoreWidth - ScoreSize.x, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.82f, 0.46f, 1.0f));
        ImGui::TextUnformatted(Buffer);
        ImGui::PopStyleColor();
    }
}