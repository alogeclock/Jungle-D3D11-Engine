#pragma once

#include "imgui/imgui.h"

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

struct FPopupFrameLayout
{
    float InnerLeft = 0.0f;
    float InnerTop = 0.0f;
    float InnerWidth = 0.0f;
    float InnerHeight = 0.0f;

    float TitleX = 0.0f;
    float TitleY = 0.0f;
    float DividerY = 0.0f;

    float ContentLeft = 0.0f;
    float ContentTop = 0.0f;
    float ContentRight = 0.0f;
    float ContentBottom = 0.0f;
    float ContentWidth = 0.0f;
    float ContentHeight = 0.0f;

    float ButtonAreaLeft = 0.0f;
    float ButtonAreaRight = 0.0f;
    float ButtonAreaTop = 0.0f;
    float ButtonAreaBottom = 0.0f;
    float ButtonY = 0.0f;

    ImVec2 TitleSize = ImVec2(0.0f, 0.0f);
};

class FUIPopupBase
{
  public:
    virtual ~FUIPopupBase() = default;

    virtual void Render(FGameContext &Context) = 0;
    virtual void Update(FGameContext &Context) {}

    void Open();
    void Close();

    bool IsOpen() const { return bIsOpen; }
    bool IsClosed() const { return !bIsOpen; }

  protected:
    static constexpr float DefaultPopupWidth = 760.0f;
    static constexpr float DefaultPopupHeight = 420.0f;

    static constexpr float WindowPaddingX = 32.0f;
    static constexpr float WindowPaddingY = 24.0f;
    static constexpr float WindowRounding = 14.0f;
    static constexpr float DimBgAlpha = 0.06f;

    static constexpr float TitleFontScale = 2.2f;
    static constexpr float ButtonTextFontScale = 1.35f;

    static constexpr float ButtonWidth = 260.0f;
    static constexpr float ButtonHeight = 48.0f;
    static constexpr float ButtonGap = 16.0f;
    static constexpr float BottomPadding = 12.0f;

    static constexpr float TitleTopOffset = 34.0f;
    static constexpr float TitleToDividerGap = 10.0f;
    static constexpr float DividerThickness = 1.5f;
    static constexpr float DividerToContentGap = 18.0f;
    static constexpr float ContentToButtonGap = 18.0f;
    static constexpr float DividerInsetX = 40.0f;

  protected:
    bool BeginPopupWindow(const char *PopupId, const char *Title, const ImVec2 &PopupSize,
                          FPopupFrameLayout &OutLayout);
    void EndPopupWindow();

    bool DrawBottomButton(const FPopupFrameLayout &Layout, const char *Label);
    bool DrawBottomButton(const FPopupFrameLayout &Layout, const char *Label, int ButtonIndex,
                          int ButtonCount);

    ImVec2 GetBottomButtonPosition(const FPopupFrameLayout &Layout, int ButtonIndex,
                                   int ButtonCount, float InButtonWidth = ButtonWidth,
                                   float InButtonHeight = ButtonHeight,
                                   float InButtonGap = ButtonGap) const;

    float GetContentFontScale(EUIPopupContentTextSize Size) const;
    float GetAlignedX(const FPopupFrameLayout &Layout, float ItemWidth,
                      EUIPopupContentAlign Align) const;

    bool ConsumeOpenRequest();

  private:
    void SetupPopupWindow(const char *PopupId, const ImVec2 &PopupSize);
    void BuildFrameLayout(const char *Title, FPopupFrameLayout &OutLayout) const;
    void DrawTitleAndDivider(const char *Title, const FPopupFrameLayout &Layout) const;

  protected:
    bool bIsOpen = false;
    bool bOpenRequested = false;
};