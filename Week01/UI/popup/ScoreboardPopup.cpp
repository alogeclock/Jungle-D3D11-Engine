#include "pch.h"
#include <cstdio>
#include "ScoreboardPopup.h"
#include "imgui/imgui.h"

void FScoreboardPopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("Scoreboard", "SCORE BOARD", ImVec2(DefaultPopupWidth, DefaultPopupHeight),
                          Layout))
        return;

    if (Entries.empty())
        DrawEmpty(Layout);
    else
        DrawEntries(Layout);

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
    const float  Y = Layout.ContentTop + (Layout.ContentHeight - TextSize.y) * 0.5f;

    ImGui::SetCursorPos(ImVec2(X, Y));
    ImGui::TextUnformatted(Text);

    ImGui::SetWindowFontScale(1.0f);
}

void FScoreboardPopup::DrawEntries(const FPopupFrameLayout &Layout)
{
    const float ContentFontScale = GetContentFontScale(ContentTextSize);
    ImGui::SetWindowFontScale(ContentFontScale);

    float MaxRankWidth = 0.0f;
    float MaxNameWidth = 0.0f;
    float MaxScoreWidth = 0.0f;
    char  Buffer[64]{};

    for (std::size_t Index = 0; Index < Entries.size(); ++Index)
    {
        std::snprintf(Buffer, sizeof(Buffer), "#%zu", Index + 1);
        const ImVec2 RankSize = ImGui::CalcTextSize(Buffer);
        const ImVec2 NameSize = ImGui::CalcTextSize(Entries[Index].Name.c_str());

        std::snprintf(Buffer, sizeof(Buffer), "%d", Entries[Index].Score);
        const ImVec2 ScoreSize = ImGui::CalcTextSize(Buffer);

        if (RankSize.x > MaxRankWidth)
            MaxRankWidth = RankSize.x;
        if (NameSize.x > MaxNameWidth)
            MaxNameWidth = NameSize.x;
        if (ScoreSize.x > MaxScoreWidth)
            MaxScoreWidth = ScoreSize.x;
    }

    const float LineHeight = ImGui::GetTextLineHeight();
    const float BlockWidth = MaxRankWidth + ColumnGap + MaxNameWidth + ColumnGap + MaxScoreWidth;
    const float BlockHeight =
        Entries.empty() ? 0.0f : Entries.size() * LineHeight + (Entries.size() - 1) * RowGap;

    const float StartX = GetAlignedX(Layout, BlockWidth, ContentAlign);
    const float StartY = Layout.ContentTop + (Layout.ContentHeight - BlockHeight) * 0.5f;

    const float NameX = StartX + MaxRankWidth + ColumnGap;
    const float ScoreX = NameX + MaxNameWidth + ColumnGap;

    for (std::size_t Index = 0; Index < Entries.size(); ++Index)
    {
        const float RowY = StartY + Index * (LineHeight + RowGap);

        std::snprintf(Buffer, sizeof(Buffer), "#%zu", Index + 1);
        ImGui::SetCursorPos(ImVec2(StartX, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.78f, 1.0f));
        ImGui::TextUnformatted(Buffer);
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(NameX, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::TextUnformatted(Entries[Index].Name.c_str());
        ImGui::PopStyleColor();

        std::snprintf(Buffer, sizeof(Buffer), "%d", Entries[Index].Score);
        const ImVec2 ScoreSize = ImGui::CalcTextSize(Buffer);
        ImGui::SetCursorPos(ImVec2(ScoreX + MaxScoreWidth - ScoreSize.x, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.82f, 0.46f, 1.0f));
        ImGui::TextUnformatted(Buffer);
        ImGui::PopStyleColor();
    }

    ImGui::SetWindowFontScale(1.0f);
}
