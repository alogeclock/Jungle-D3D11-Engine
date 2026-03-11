#pragma once

#include <string>
#include <vector>
#include "UIPopupBase.h"

struct FScoreRecord
{
    std::string Nickname;
    int         Stage = 0;
    int         Score = 0;
};

class FScoreboardPopup : public FUIPopupBase
{
  public:
    void SetRecords(const std::vector<FScoreRecord> &InRecords) { Records = InRecords; }

    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    static constexpr EUIPopupContentAlign    ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Medium;

    static constexpr std::size_t MaxRowsPerColumn = 6;

    static constexpr float ColumnGap = 18.0f;
    static constexpr float RowGap = 10.0f;
    static constexpr float SectionGap = 24.0f;
    static constexpr float DividerThickness = 1.0f;
    static constexpr float DividerMarginY = 0.0f;

  private:
    void DrawRecords(const FPopupFrameLayout &Layout);
    void DrawEmpty(const FPopupFrameLayout &Layout);
    void DrawRecordColumn(const ImVec2 &StartPos, const std::vector<FScoreRecord> &ColumnRecords);

  private:
    std::vector<FScoreRecord> Records = {
        {"YEONHA", 4, 12500},
        {"HOJIN", 3, 9800},
        {"WOOJIN", 2, 8700},
        {"HYNGIL", 5, 7600},
        {"MINSEO", 2, 6900},
        {"JIWON", 1, 5400},
        {"JIWON", 1, 5400},
        {"JIWON", 1, 5400},
        {"JIWON", 1, 5400},
        {"JIWON", 1, 5400},
        {"JIWON", 1, 5400},
        {"YEONHA", 4, 12500},
        {"YEONHA", 4, 12500},
        {"YEONHA", 4, 12500},
        {"YEONHA", 4, 12500},
        {"YEONHA", 4, 12500}
    };
};
