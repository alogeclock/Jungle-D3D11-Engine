#include "pch.h"
#include "UIPopupBase.h"
#include "imgui/imgui.h"

void FUIPopupBase::Open()
{
    bIsOpen = true;
    bOpenRequested = true;
}

void FUIPopupBase::Close()
{
    bIsOpen = false;
    bOpenRequested = false;
}

bool FUIPopupBase::ConsumeOpenRequest()
{
    const bool bResult = bOpenRequested;
    bOpenRequested = false;
    return bResult;
}

void FUIPopupBase::SetupPopupWindow(const char *PopupId, const ImVec2 &PopupSize)
{
    ImGuiIO &Io = ImGui::GetIO();
    const ImVec2 PopupPos(Io.DisplaySize.x * 0.5f, Io.DisplaySize.y * 0.5f);

    if (ConsumeOpenRequest())
    {
        ImGui::SetNextWindowSize(PopupSize, ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(PopupPos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup(PopupId);
    }

    ImGui::SetNextWindowSize(PopupSize, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(PopupPos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

bool FUIPopupBase::BeginPopupWindow(const char *PopupId, const char *Title, const ImVec2 &PopupSize,
                                    FPopupFrameLayout &OutLayout)
{
    if (!bIsOpen)
        return false;

    SetupPopupWindow(PopupId, PopupSize);

    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, DimBgAlpha));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(WindowPaddingX, WindowPaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, WindowRounding);

    if (!ImGui::BeginPopupModal(PopupId, nullptr,
                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        return false;
    }

    BuildFrameLayout(Title, OutLayout);
    DrawTitleAndDivider(Title, OutLayout);
    return true;
}

void FUIPopupBase::EndPopupWindow()
{
    ImGui::EndPopup();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void FUIPopupBase::BuildFrameLayout(const char *Title, FPopupFrameLayout &OutLayout) const
{
    ImGuiStyle &Style = ImGui::GetStyle();
    const ImVec2 WindowSize = ImGui::GetWindowSize();

    OutLayout.InnerLeft = Style.WindowPadding.x;
    OutLayout.InnerTop = Style.WindowPadding.y;
    OutLayout.InnerWidth = WindowSize.x - Style.WindowPadding.x * 2.0f;
    OutLayout.InnerHeight = WindowSize.y - Style.WindowPadding.y * 2.0f;

    OutLayout.ButtonX = OutLayout.InnerLeft + (OutLayout.InnerWidth - ButtonWidth) * 0.5f;
    OutLayout.ButtonY = OutLayout.InnerTop + OutLayout.InnerHeight - ButtonHeight - BottomPadding;

    ImGui::SetWindowFontScale(TitleFontScale);
    OutLayout.TitleSize = ImGui::CalcTextSize(Title);
    ImGui::SetWindowFontScale(1.0f);

    OutLayout.TitleX = OutLayout.InnerLeft + (OutLayout.InnerWidth - OutLayout.TitleSize.x) * 0.5f;
    OutLayout.TitleY = OutLayout.InnerTop + TitleTopOffset;
    OutLayout.DividerY = OutLayout.TitleY + OutLayout.TitleSize.y + TitleToDividerGap;

    OutLayout.ContentLeft = OutLayout.InnerLeft;
    OutLayout.ContentRight = OutLayout.InnerLeft + OutLayout.InnerWidth;
    OutLayout.ContentTop = OutLayout.DividerY + DividerThickness + DividerToContentGap;
    OutLayout.ContentBottom = OutLayout.ButtonY - ContentToButtonGap;
    OutLayout.ContentWidth = OutLayout.ContentRight - OutLayout.ContentLeft;
    OutLayout.ContentHeight = OutLayout.ContentBottom - OutLayout.ContentTop;
}

void FUIPopupBase::DrawTitleAndDivider(const char *Title, const FPopupFrameLayout &Layout) const
{
    ImGui::SetWindowFontScale(TitleFontScale);
    ImGui::SetCursorPos(ImVec2(Layout.TitleX, Layout.TitleY));
    ImGui::TextUnformatted(Title);
    ImGui::SetWindowFontScale(1.0f);

    const ImVec2 WindowPos = ImGui::GetWindowPos();
    const float DividerStartX = WindowPos.x + Layout.InnerLeft + DividerInsetX;
    const float DividerEndX = WindowPos.x + Layout.ContentRight - DividerInsetX;
    const float DividerY = WindowPos.y + Layout.DividerY;

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    DrawList->AddLine(ImVec2(DividerStartX, DividerY), ImVec2(DividerEndX, DividerY),
                      IM_COL32(180, 180, 180, 160), DividerThickness);
}

bool FUIPopupBase::DrawBottomButton(const FPopupFrameLayout &Layout, const char *Label)
{
    ImGui::SetWindowFontScale(ButtonTextFontScale);
    ImGui::SetCursorPos(ImVec2(Layout.ButtonX, Layout.ButtonY));
    const bool bPressed = ImGui::Button(Label, ImVec2(ButtonWidth, ButtonHeight));
    ImGui::SetWindowFontScale(1.0f);
    return bPressed;
}

float FUIPopupBase::GetContentFontScale(EUIPopupContentTextSize Size) const
{
    switch (Size)
    {
    case EUIPopupContentTextSize::Small:
        return 1.25f;
    case EUIPopupContentTextSize::Medium:
        return 1.55f;
    case EUIPopupContentTextSize::Big:
        return 1.9f;
    }

    return 1.55f;
}

float FUIPopupBase::GetAlignedX(const FPopupFrameLayout &Layout, float ItemWidth,
                                EUIPopupContentAlign Align) const
{
    switch (Align)
    {
    case EUIPopupContentAlign::Left:
        return Layout.ContentLeft;
    case EUIPopupContentAlign::Center:
        return Layout.ContentLeft + (Layout.ContentWidth - ItemWidth) * 0.5f;
    case EUIPopupContentAlign::Right:
        return Layout.ContentRight - ItemWidth;
    }

    return Layout.ContentLeft;
}
