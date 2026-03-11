#pragma once

#include "UIPopupAction.h"
#include "imgui/imgui.h"
#include <initializer_list>

struct FGameContext;

enum class EUIPopupContentAlign
{
    Left,
    Center,
    Right
};

enum class EUIPopupContentTextSize
{
    Small,
    Medium,
    Big
};

enum class EUIPopupContentVerticalAlign
{
    Top,
    Center
};

struct FPopupFrameLayout
{
    float InnerLeft = 0.0f;
    float InnerTop = 0.0f;
    float InnerWidth = 0.0f;
    float InnerHeight = 0.0f;

    ImVec2 TitleSize = ImVec2(0.0f, 0.0f);
    float  TitleX = 0.0f;
    float  TitleY = 0.0f;
    float  DividerY = 0.0f;

    float ContentLeft = 0.0f;
    float ContentRight = 0.0f;
    float ContentTop = 0.0f;
    float ContentBottom = 0.0f;
    float ContentWidth = 0.0f;
    float ContentHeight = 0.0f;

    float ButtonAreaLeft = 0.0f;
    float ButtonAreaRight = 0.0f;
    float ButtonAreaTop = 0.0f;
    float ButtonAreaBottom = 0.0f;
    float ButtonY = 0.0f;
};

class FUIPopupBase
{
  public:
    virtual ~FUIPopupBase() = default;

    void Open();
    void Close();

    bool IsOpen() const { return bIsOpen; }
    bool IsClosed() const { return !bIsOpen; }

    virtual void Render(FGameContext &Context) = 0;
    virtual void Update(FGameContext &Context) = 0;

  protected:
    bool ConsumeOpenRequest();

    void SetupPopupWindow(const char *PopupId, const ImVec2 &PopupSize);
    bool BeginPopupWindow(const char *PopupId, const char *Title, const ImVec2 &PopupSize,
                          FPopupFrameLayout &OutLayout);
    void EndPopupWindow();

    void BuildFrameLayout(const char *Title, FPopupFrameLayout &OutLayout) const;
    void DrawTitleAndDivider(const char *Title, const FPopupFrameLayout &Layout) const;

    float GetBottomButtonWidth(const FPopupFrameLayout &Layout, int ButtonCount,
                               float InButtonWidth = ButtonWidth,
                               float InButtonGap = ButtonGap) const;

    ImVec2 GetBottomButtonPosition(const FPopupFrameLayout &Layout, int ButtonIndex,
                                   int ButtonCount, float InButtonWidth = ButtonWidth,
                                   float InButtonHeight = ButtonHeight,
                                   float InButtonGap = ButtonGap) const;

    bool DrawBottomButton(const FPopupFrameLayout &Layout, const char *Label);
    bool DrawBottomButton(const FPopupFrameLayout &Layout, const char *Label, int ButtonIndex,
                          int ButtonCount);

    float GetContentFontScale(EUIPopupContentTextSize Size) const;
    float GetAlignedX(const FPopupFrameLayout &Layout, float ItemWidth,
                      EUIPopupContentAlign Align) const;

    float GetTextBlockHeight(const char *const *Lines, int LineCount, float LineGap,
                             EUIPopupContentTextSize TextSize) const;

    void DrawTextBlock(const FPopupFrameLayout &Layout, const char *const *Lines, int LineCount,
                       float LineGap, EUIPopupContentAlign HorizontalAlign,
                       EUIPopupContentTextSize TextSize,
                       EUIPopupContentVerticalAlign VerticalAlign =
                           EUIPopupContentVerticalAlign::Center) const;

    void DrawTextBlock(const FPopupFrameLayout &Layout,
                       std::initializer_list<const char *> Lines,
                       float LineGap, EUIPopupContentAlign HorizontalAlign,
                       EUIPopupContentTextSize TextSize,
                       EUIPopupContentVerticalAlign VerticalAlign =
                           EUIPopupContentVerticalAlign::Center) const;

  protected:
    static constexpr float DefaultPopupWidth = 760.0f;
    static constexpr float DefaultPopupHeight = 420.0f;

    static constexpr float WindowPaddingX = 20.0f;
    static constexpr float WindowPaddingY = 18.0f;
    static constexpr float WindowRounding = 12.0f;
    static constexpr float DimBgAlpha = 0.35f;

    static constexpr float TitleFontScale = 1.8f;
    static constexpr float TitleTopOffset = 18.0f;
    static constexpr float TitleToDividerGap = 10.0f;
    static constexpr float DividerInsetX = 8.0f;
    static constexpr float DividerThickness = 1.0f;
    static constexpr float DividerToContentGap = 16.0f;

    static constexpr float BottomPadding = 6.0f;
    static constexpr float ContentToButtonGap = 16.0f;

    static constexpr float ButtonWidth = 240.0f;
    static constexpr float ButtonHeight = 40.0f;
    static constexpr float ButtonGap = 12.0f;
    static constexpr float ButtonTextFontScale = 1.3f;
    static constexpr float MinButtonWidthWhenMulti = 80.0f;

    static constexpr float DefaultContentLineGap = 6.0f;

  private:
    bool bIsOpen = false;
    bool bOpenRequested = false;
};
