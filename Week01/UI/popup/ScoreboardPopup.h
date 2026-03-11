#pragma once

#include <string>
#include <vector>
#include "UIPopupBase.h"

struct FScoreboardEntry
{
    std::string Name;
    int Score = 0;
};

class FScoreboardPopup : public FUIPopupBase
{
  public:
    void SetEntries(const std::vector<FScoreboardEntry> &InEntries) { Entries = InEntries; }

    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    static constexpr EUIPopupContentAlign ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Medium;
    static constexpr float ColumnGap = 26.0f;
    static constexpr float RowGap = 12.0f;

  private:
    void DrawEntries(const FPopupFrameLayout &Layout);
    void DrawEmpty(const FPopupFrameLayout &Layout);

  private:
    std::vector<FScoreboardEntry> Entries = {
        {"KIM YEONHA", 12500},
        {"LEE HOJIN", 9800},
        {"SIM WOOJIN", 8700},
        {"JEON HYUNGIL", 7600}
    };
};
