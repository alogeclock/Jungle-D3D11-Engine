#include "pch.h"
#include "UIPopupBase.h"
#include "imgui/imgui.h"
#include <initializer_list>
#include "Core/AudioSystem.h"

namespace
{
    float GetActualButtonHeight() { return ImGui::GetFrameHeight(); }

    float GetScaledTextHeight(float FontScale) { return ImGui::GetFontSize() * FontScale; }
} // namespace

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
    ImGuiIO     &Io = ImGui::GetIO();
    const ImVec2 PopupPos(Io.DisplaySize.x * 0.5f, Io.DisplaySize.y * 0.5f);

    if (ConsumeOpenRequest())
    {
        ImGui::SetNextWindowSize(PopupSize, ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(PopupPos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup(PopupId);

        FAudioSystem::Get().LoadWav("PopupSound", "Resources/Sounds/Popup.wav");
        FAudioSystem::Get().Play("PopupSound");
    }

    ImGui::SetNextWindowSize(PopupSize, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(PopupPos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
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
                                    ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_NoScrollWithMouse))
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
    ImGuiStyle  &Style = ImGui::GetStyle();
    const ImVec2 WindowSize = ImGui::GetWindowSize();
    const float  ActualButtonHeight = GetActualButtonHeight();

    OutLayout.InnerLeft = Style.WindowPadding.x;
    OutLayout.InnerTop = Style.WindowPadding.y;
    OutLayout.InnerWidth = WindowSize.x - Style.WindowPadding.x * 2.0f;
    OutLayout.InnerHeight = WindowSize.y - Style.WindowPadding.y * 2.0f;

    ImGui::SetWindowFontScale(TitleFontScale);
    OutLayout.TitleSize = ImGui::CalcTextSize(Title);
    ImGui::SetWindowFontScale(1.0f);

    if (OutLayout.TitleSize.y < GetScaledTextHeight(TitleFontScale))
        OutLayout.TitleSize.y = GetScaledTextHeight(TitleFontScale);

    OutLayout.TitleX = OutLayout.InnerLeft + (OutLayout.InnerWidth - OutLayout.TitleSize.x) * 0.5f;
    OutLayout.TitleY = OutLayout.InnerTop + TitleTopOffset;
    OutLayout.DividerY = OutLayout.TitleY + OutLayout.TitleSize.y + TitleToDividerGap;

    OutLayout.ButtonAreaLeft = OutLayout.InnerLeft;
    OutLayout.ButtonAreaRight = OutLayout.InnerLeft + OutLayout.InnerWidth;
    OutLayout.ButtonAreaBottom = OutLayout.InnerTop + OutLayout.InnerHeight - BottomPadding;
    OutLayout.ButtonAreaTop = OutLayout.ButtonAreaBottom - ActualButtonHeight;
    OutLayout.ButtonY = OutLayout.ButtonAreaTop;

    OutLayout.ContentLeft = OutLayout.InnerLeft;
    OutLayout.ContentRight = OutLayout.InnerLeft + OutLayout.InnerWidth;
    OutLayout.ContentTop = OutLayout.DividerY + DividerThickness + DividerToContentGap;
    OutLayout.ContentBottom = OutLayout.ButtonAreaTop - ContentToButtonGap;
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
    const float  DividerStartX = WindowPos.x + Layout.InnerLeft + DividerInsetX;
    const float  DividerEndX = WindowPos.x + Layout.ContentRight - DividerInsetX;
    const float  DividerY = WindowPos.y + Layout.DividerY;

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    DrawList->AddLine(ImVec2(DividerStartX, DividerY), ImVec2(DividerEndX, DividerY),
                      IM_COL32(180, 180, 180, 160), DividerThickness);
}

float FUIPopupBase::GetBottomButtonWidth(const FPopupFrameLayout &Layout, int ButtonCount,
                                         float InButtonWidth, float InButtonGap) const
{
    if (ButtonCount <= 0)
        return InButtonWidth;

    if (ButtonCount < 3)
        return InButtonWidth;

    const float TotalGapWidth = InButtonGap * static_cast<float>(ButtonCount - 1);
    const float AvailableWidth = Layout.InnerWidth - TotalGapWidth;

    if (AvailableWidth <= 0.0f)
        return InButtonWidth;

    const float FittedWidth = AvailableWidth / static_cast<float>(ButtonCount);

    if (FittedWidth < InButtonWidth)
        return FittedWidth;

    return InButtonWidth;
}

ImVec2 FUIPopupBase::GetBottomButtonPosition(const FPopupFrameLayout &Layout, int ButtonIndex,
                                             int ButtonCount, float InButtonWidth,
                                             float InButtonHeight, float InButtonGap) const
{
    if (ButtonCount <= 0)
        return ImVec2(Layout.ButtonAreaLeft, Layout.ButtonY);

    const float ActualButtonWidth =
        GetBottomButtonWidth(Layout, ButtonCount, InButtonWidth, InButtonGap);

    const float ActualButtonHeight =
        (InButtonHeight > 0.0f) ? InButtonHeight : GetActualButtonHeight();

    const float TotalWidth = ActualButtonWidth * static_cast<float>(ButtonCount) +
                             InButtonGap * static_cast<float>(ButtonCount - 1);

    const float StartX = Layout.ButtonAreaLeft + (Layout.InnerWidth - TotalWidth) * 0.5f;
    const float X = StartX + static_cast<float>(ButtonIndex) * (ActualButtonWidth + InButtonGap);
    const float Y = Layout.ButtonY + (GetActualButtonHeight() - ActualButtonHeight) * 0.5f;

    return ImVec2(X, Y);
}

bool FUIPopupBase::DrawBottomButton(const FPopupFrameLayout &Layout, const char *Label)
{
    return DrawBottomButton(Layout, Label, 0, 1);
}

bool FUIPopupBase::DrawBottomButton(const FPopupFrameLayout &Layout, const char *Label,
                                    int ButtonIndex, int ButtonCount)
{
    ImGui::SetWindowFontScale(ButtonTextFontScale);

    const float ActualButtonWidth =
        GetBottomButtonWidth(Layout, ButtonCount, ButtonWidth, ButtonGap);

    const ImVec2 ButtonPos = GetBottomButtonPosition(Layout, ButtonIndex, ButtonCount, ButtonWidth,
                                                     ButtonHeight, ButtonGap);

    ImGui::SetCursorPos(ButtonPos);
    const bool bPressed = ImGui::Button(Label, ImVec2(ActualButtonWidth, ButtonHeight));

    if (bPressed)
    {
        FAudioSystem::Get().LoadWav("PopupSound", "Resources/Sounds/Popup.wav");
        FAudioSystem::Get().Play("PopupSound");
    }

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

float FUIPopupBase::GetTextBlockHeight(const char *const *Lines, int LineCount, float LineGap,
                                       EUIPopupContentTextSize TextSize) const
{
    if (Lines == nullptr || LineCount <= 0)
        return 0.0f;

    const float FontScale = GetContentFontScale(TextSize);

    ImGui::SetWindowFontScale(FontScale);

    float TotalHeight = 0.0f;
    for (int i = 0; i < LineCount; ++i)
    {
        const char *Line = Lines[i] ? Lines[i] : "";
        ImVec2      LineSize = ImGui::CalcTextSize(Line);

        if (LineSize.y < GetScaledTextHeight(FontScale))
            LineSize.y = GetScaledTextHeight(FontScale);

        TotalHeight += LineSize.y;

        if (i + 1 < LineCount)
            TotalHeight += LineGap;
    }

    ImGui::SetWindowFontScale(1.0f);
    return TotalHeight;
}

void FUIPopupBase::DrawTextBlock(const FPopupFrameLayout &Layout, const char *const *Lines,
                                 int LineCount, float LineGap, EUIPopupContentAlign HorizontalAlign,
                                 EUIPopupContentTextSize      TextSize,
                                 EUIPopupContentVerticalAlign VerticalAlign) const
{
    if (Lines == nullptr || LineCount <= 0)
        return;

    const float FontScale = GetContentFontScale(TextSize);
    const float BlockHeight = GetTextBlockHeight(Lines, LineCount, LineGap, TextSize);

    float StartY = Layout.ContentTop;
    if (VerticalAlign == EUIPopupContentVerticalAlign::Center)
        StartY = Layout.ContentTop + (Layout.ContentHeight - BlockHeight) * 0.5f;

    ImGui::SetWindowFontScale(FontScale);

    for (int i = 0; i < LineCount; ++i)
    {
        const char  *Line = Lines[i] ? Lines[i] : "";
        const ImVec2 LineSize = ImGui::CalcTextSize(Line);
        const float  X = GetAlignedX(Layout, LineSize.x, HorizontalAlign);

        if (i == 0)
        {
            ImGui::SetCursorPos(ImVec2(X, StartY));
        }
        else
        {
            ImGui::SetCursorPosX(X);
        }

        ImGui::TextUnformatted(Line);

        if (i + 1 < LineCount)
        {
            ImGui::Dummy(ImVec2(0.0f, LineGap));
        }
    }

    ImGui::SetWindowFontScale(1.0f);
}

void FUIPopupBase::DrawTextBlock(const FPopupFrameLayout            &Layout,
                                 std::initializer_list<const char *> Lines, float LineGap,
                                 EUIPopupContentAlign         HorizontalAlign,
                                 EUIPopupContentTextSize      TextSize,
                                 EUIPopupContentVerticalAlign VerticalAlign) const
{
    if (Lines.size() == 0)
        return;

    DrawTextBlock(Layout, Lines.begin(), static_cast<int>(Lines.size()), LineGap, HorizontalAlign,
                  TextSize, VerticalAlign);
}