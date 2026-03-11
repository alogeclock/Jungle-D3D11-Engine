#include "pch.h"
#include "CreditPopup.h"
#include "imgui/imgui.h"

void FCreditPopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("Credits", "CREDITS", ImVec2(DefaultPopupWidth, DefaultPopupHeight),
                          Layout))
        return;

    DrawCredits(Layout);

    if (DrawBottomButton(Layout, "Close"))
    {
        ImGui::CloseCurrentPopup();
        Close();
    }

    EndPopupWindow();
}

void FCreditPopup::DrawCredits(const FPopupFrameLayout &Layout)
{
    const float ContentFontScale = GetContentFontScale(ContentTextSize);
    ImGui::SetWindowFontScale(ContentFontScale);

    float MaxNameWidth = 0.0f;
    float MaxRoleWidth = 0.0f;

    for (const FCreditEntry &Entry : Credits)
    {
        const ImVec2 NameSize = ImGui::CalcTextSize(Entry.Name.c_str());
        const ImVec2 RoleSize = ImGui::CalcTextSize(Entry.Role.c_str());

        if (NameSize.x > MaxNameWidth)
            MaxNameWidth = NameSize.x;
        if (RoleSize.x > MaxRoleWidth)
            MaxRoleWidth = RoleSize.x;
    }

    const float PipeWidth = ImGui::CalcTextSize("|").x;
    const float LineHeight = ImGui::GetTextLineHeight();
    const float BlockWidth = MaxNameWidth + PipeGap + PipeWidth + PipeGap + MaxRoleWidth;
    const float BlockHeight =
        Credits.empty() ? 0.0f : Credits.size() * LineHeight + (Credits.size() - 1) * LineGap;

    const float StartX = GetAlignedX(Layout, BlockWidth, ContentAlign) + BlockOffsetX;
    const float StartY = Layout.ContentTop + (Layout.ContentHeight - BlockHeight) * 0.5f;
    const float PipeX = StartX + MaxNameWidth + PipeGap;
    const float RoleX = PipeX + PipeWidth + PipeGap;

    for (std::size_t Index = 0; Index < Credits.size(); ++Index)
    {
        const FCreditEntry &Entry = Credits[Index];
        const ImVec2        NameSize = ImGui::CalcTextSize(Entry.Name.c_str());
        const float         RowY = StartY + Index * (LineHeight + LineGap);

        ImGui::SetCursorPos(ImVec2(PipeX - PipeGap - NameSize.x, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::TextUnformatted(Entry.Name.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(PipeX, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.82f, 1.0f));
        ImGui::TextUnformatted("|");
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(RoleX, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.72f, 1.0f));
        ImGui::TextUnformatted(Entry.Role.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SetWindowFontScale(1.0f);
}
