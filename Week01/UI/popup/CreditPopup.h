#pragma once

#include <string>
#include <vector>
#include "UIPopupBase.h"

struct FCreditEntry
{
    std::string Name;
    std::string Role;
};

struct FCreditPopupLayout
{
    float InnerLeft = 0.0f;
    float InnerTop = 0.0f;
    float InnerWidth = 0.0f;
    float InnerHeight = 0.0f;

    float TitleX = 0.0f;
    float TitleY = 0.0f;
    float DividerY = 0.0f;

    float CreditsStartX = 0.0f;
    float CreditsStartY = 0.0f;
    float PipeX = 0.0f;
    float RoleX = 0.0f;

    float ButtonX = 0.0f;
    float ButtonY = 0.0f;

    float  MaxNameWidth = 0.0f;
    float  MaxRoleWidth = 0.0f;
    float  PipeWidth = 0.0f;
    float  LineHeight = 0.0f;
    float  CreditsHeight = 0.0f;
    ImVec2 TitleSize = ImVec2(0.0f, 0.0f);
};

class FCreditPopup : public FUIPopupBase
{
  public:
    void SetCredits(const std::vector<FCreditEntry> &InCredits) { Credits = InCredits; }

    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    static constexpr float PopupWidth = 760.0f;
    static constexpr float PopupHeight = 420.0f;

    static constexpr float TitleFontScale = 2.2f;
    static constexpr float CreditFontScale = 1.9f;

    static constexpr float ButtonWidth = 260.0f;
    static constexpr float ButtonHeight = 48.0f;
    static constexpr float BottomPadding = 12.0f;

    static constexpr float TitleToDividerGap = 10.0f;
    static constexpr float DividerToCreditsGap = 18.0f;
    static constexpr float DividerThickness = 1.5f;
    static constexpr float LineGap = 14.0f;
    static constexpr float PipeGap = 18.0f;
    static constexpr float BlockOffsetX = -10.0f;

  private:
    void               SetupPopup();
    FCreditPopupLayout CalculateLayout() const;

    void DrawTitle(const FCreditPopupLayout &Layout) const;
    void DrawDivider(const FCreditPopupLayout &Layout) const;
    void DrawCredits(const FCreditPopupLayout &Layout) const;
    void DrawCloseButton();

  private:
    std::vector<FCreditEntry> Credits = {{"KIM YEONHA", "GAME DESIGN"},
                                         {"LEE HOJIN", "GAMEPLAY PROGRAMMING"},
                                         {"SIM WOOJIN", "UI DESIGN"},
                                         {"JEON HYUNGIL", "LEVEL DESIGN"}};
};

#include "CreditPopup.h"
#include "imgui/imgui.h"
#include "pch.h"

void FCreditPopup::Render(FGameContext &Context)
{
    SetupPopup();

    if (!bIsOpen)
        return;

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32.0f, 24.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);

    if (ImGui::BeginPopupModal("Credits", nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse))
    {
        const FCreditPopupLayout Layout = CalculateLayout();

        DrawTitle(Layout);
        DrawDivider(Layout);
        DrawCredits(Layout);
        DrawCloseButton();

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void FCreditPopup::SetupPopup()
{
    ImGuiIO     &Io = ImGui::GetIO();
    const ImVec2 PopupSize(PopupWidth, PopupHeight);
    const ImVec2 PopupPos(Io.DisplaySize.x * 0.5f, Io.DisplaySize.y * 0.5f);

    if (ConsumeOpenRequest())
    {
        ImGui::SetNextWindowSize(PopupSize, ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(PopupPos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Credits");
    }

    ImGui::SetNextWindowSize(PopupSize, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(PopupPos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

FCreditPopupLayout FCreditPopup::CalculateLayout() const
{
    FCreditPopupLayout Layout;

    ImGuiStyle  &Style = ImGui::GetStyle();
    const ImVec2 WindowSize = ImGui::GetWindowSize();

    Layout.InnerLeft = Style.WindowPadding.x;
    Layout.InnerTop = Style.WindowPadding.y;
    Layout.InnerWidth = WindowSize.x - Style.WindowPadding.x * 2.0f;
    Layout.InnerHeight = WindowSize.y - Style.WindowPadding.y * 2.0f;

    Layout.ButtonX = Layout.InnerLeft + (Layout.InnerWidth - ButtonWidth) * 0.5f;
    Layout.ButtonY = Layout.InnerTop + Layout.InnerHeight - ButtonHeight - BottomPadding;

    ImGui::SetWindowFontScale(TitleFontScale);
    Layout.TitleSize = ImGui::CalcTextSize("CREDITS");

    ImGui::SetWindowFontScale(CreditFontScale);

    for (const FCreditEntry &Entry : Credits)
    {
        const ImVec2 NameSize = ImGui::CalcTextSize(Entry.Name.c_str());
        const ImVec2 RoleSize = ImGui::CalcTextSize(Entry.Role.c_str());

        if (NameSize.x > Layout.MaxNameWidth)
            Layout.MaxNameWidth = NameSize.x;
        if (RoleSize.x > Layout.MaxRoleWidth)
            Layout.MaxRoleWidth = RoleSize.x;
    }

    Layout.PipeWidth = ImGui::CalcTextSize("|").x;
    Layout.LineHeight = ImGui::GetTextLineHeight();
    Layout.CreditsHeight =
        Credits.empty() ? 0.0f
                        : Credits.size() * Layout.LineHeight + (Credits.size() - 1) * LineGap;

    const float CreditsWidth =
        Layout.MaxNameWidth + PipeGap + Layout.PipeWidth + PipeGap + Layout.MaxRoleWidth;

    const float SectionGap = 18.0f;

    Layout.TitleX = Layout.InnerLeft + (Layout.InnerWidth - Layout.TitleSize.x) * 0.5f;
    Layout.CreditsStartX =
        Layout.InnerLeft + (Layout.InnerWidth - CreditsWidth) * 0.5f + BlockOffsetX;

    Layout.TitleY = Layout.InnerTop + 36.0f;
    Layout.DividerY = Layout.TitleY + Layout.TitleSize.y + TitleToDividerGap;

    const float CreditsAreaTop = Layout.DividerY + DividerThickness;
    const float CreditsAreaBottom = Layout.ButtonY - SectionGap;
    const float CreditsAreaHeight = CreditsAreaBottom - CreditsAreaTop;

    const float CreditsOffsetY = (CreditsAreaHeight - Layout.CreditsHeight) * 0.5f;
    Layout.CreditsStartY = CreditsAreaTop + CreditsOffsetY;

    Layout.PipeX = Layout.CreditsStartX + Layout.MaxNameWidth + PipeGap;
    Layout.RoleX = Layout.PipeX + Layout.PipeWidth + PipeGap;

    ImGui::SetWindowFontScale(1.0f);
    return Layout;
}

void FCreditPopup::DrawTitle(const FCreditPopupLayout &Layout) const
{
    ImGui::SetWindowFontScale(TitleFontScale);
    ImGui::SetCursorPos(ImVec2(Layout.TitleX, Layout.TitleY));
    ImGui::TextUnformatted("CREDITS");
    ImGui::SetWindowFontScale(1.0f);
}

void FCreditPopup::DrawDivider(const FCreditPopupLayout &Layout) const
{
    const ImVec2 WindowPos = ImGui::GetWindowPos();
    const float  DividerStartX = WindowPos.x + Layout.InnerLeft + 40.0f;
    const float  DividerEndX = WindowPos.x + Layout.InnerLeft + Layout.InnerWidth - 40.0f;
    const float  DividerY = WindowPos.y + Layout.DividerY;

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    DrawList->AddLine(ImVec2(DividerStartX, DividerY), ImVec2(DividerEndX, DividerY),
                      IM_COL32(180, 180, 180, 160), DividerThickness);
}

void FCreditPopup::DrawCredits(const FCreditPopupLayout &Layout) const
{
    ImGui::SetWindowFontScale(CreditFontScale);

    for (std::size_t Index = 0; Index < Credits.size(); ++Index)
    {
        const FCreditEntry &Entry = Credits[Index];
        const ImVec2        NameSize = ImGui::CalcTextSize(Entry.Name.c_str());
        const float         RowY = Layout.CreditsStartY + Index * (Layout.LineHeight + LineGap);

        ImGui::SetCursorPos(ImVec2(Layout.PipeX - PipeGap - NameSize.x, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::TextUnformatted(Entry.Name.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(Layout.PipeX, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.82f, 1.0f));
        ImGui::TextUnformatted("|");
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(Layout.RoleX, RowY));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.72f, 1.0f));
        ImGui::TextUnformatted(Entry.Role.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SetWindowFontScale(1.0f);
}

void FCreditPopup::DrawCloseButton()
{
    const ImVec2 WindowSize = ImGui::GetWindowSize();
    ImGuiStyle  &Style = ImGui::GetStyle();

    const float InnerLeft = Style.WindowPadding.x;
    const float InnerTop = Style.WindowPadding.y;
    const float InnerWidth = WindowSize.x - Style.WindowPadding.x * 2.0f;
    const float InnerHeight = WindowSize.y - Style.WindowPadding.y * 2.0f;

    const float ButtonX = InnerLeft + (InnerWidth - ButtonWidth) * 0.5f;
    const float ButtonY = InnerTop + InnerHeight - ButtonHeight - BottomPadding;

    ImGui::SetCursorPos(ImVec2(ButtonX, ButtonY));

    if (ImGui::Button("Close", ImVec2(ButtonWidth, ButtonHeight)))
    {
        ImGui::CloseCurrentPopup();
        Close();
    }
}