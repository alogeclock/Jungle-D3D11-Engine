#pragma once

#include <string>
#include <vector>
#include "UIPopupAction.h"
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
    void SetEntries(const std::vector<FScoreRecord> &InEntries);
    void GoToNextPage();
    void ResetPage();

    EUIPopupAction ConsumeAction();
    void           Render(FGameContext &Context) override;
    void           Update(FGameContext &Context) override {};

  private:
    int  GetTotalPages() const;
    int  GetPageStartIndex() const;
    int  GetPageEntryCount() const;
    void DrawEntries(const FPopupFrameLayout &Layout);
    void DrawPageText(const FPopupFrameLayout &Layout);
    void DrawBottomButtons(const FPopupFrameLayout &Layout, bool bHasPrevPage, bool bHasNextPage);

  private:
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Big;
    static constexpr int                     MaxRowsPerColumn = 4;
    static constexpr int                     ColumnCount = 2;
    static constexpr int                     EntriesPerPage = MaxRowsPerColumn * ColumnCount;
    static constexpr float                   ColumnGap = 36.0f;
    static constexpr float                   RowGap = 8.0f;

    static constexpr float RankColumnWidth = 42.0f;
    static constexpr float NameColumnWidth = 90.0f;
    static constexpr float StageColumnWidth = 90.0f;

  private:
    std::vector<FScoreRecord> Entries = {
        {"A", 1, 12500},     {"BB", 2, 11800},     {"CAT", 3, 11200},  {"DOGE", 2, 10900},
        {"ECHO5", 4, 10450}, {"FOX777", 5, 10000}, {"G", 1, 9650},     {"HI", 3, 9400},
        {"ICE", 4, 9150},    {"JAZZ", 2, 8900},    {"KILO5", 6, 8700}, {"LMN123", 7, 8450},
        {"M", 2, 8300},      {"NO", 5, 8100},      {"OWL", 3, 7900},   {"PINK", 4, 7700},
        {"QWER5", 6, 7550},  {"RABBIT", 7, 7400},  {"S", 1, 7200},     {"TT", 2, 7050},
        {"UNO", 3, 6900},    {"VOLT", 5, 6700},    {"WAVE9", 6, 6550}, {"XYZ123", 7, 6400},
        {"Y", 2, 6200},      {"ZZ", 4, 6050},      {"AAA", 5, 5900},   {"BING", 3, 5750},
        {"CLOUD5", 6, 5600}, {"D", 1, 5450}};

    int            CurrentPage = 0;
    EUIPopupAction PendingAction = EUIPopupAction::None;
};
