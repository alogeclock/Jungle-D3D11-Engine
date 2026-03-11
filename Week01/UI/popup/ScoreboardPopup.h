#pragma once

#include <string>
#include <vector>
#include "UIPopupAction.h"
#include "UIPopupBase.h"

struct FScoreboardEntry
{
    std::string Name;
    int Score = 0;
};

class FScoreboardPopup : public FUIPopupBase
{
  public:
    void SetEntries(const std::vector<FScoreboardEntry> &InEntries);
    void GoToNextPage();
    void ResetPage();

    EUIPopupAction ConsumeAction();
    void Render(FGameContext &Context) override;

  private:
    int GetTotalPages() const;
    int GetPageStartIndex() const;
    int GetPageEntryCount() const;
    void DrawEntries(const FPopupFrameLayout &Layout);
    void DrawPageText(const FPopupFrameLayout &Layout);

  private:
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Medium;
    static constexpr int MaxRowsPerColumn = 7;
    static constexpr int ColumnCount = 2;
    static constexpr int EntriesPerPage = MaxRowsPerColumn * ColumnCount;
    static constexpr float ColumnGap = 36.0f;
    static constexpr float RowGap = 8.0f;
    static constexpr float RankColumnWidth = 42.0f;
    static constexpr float NameColumnWidth = 120.0f;

  private:
    std::vector<FScoreboardEntry> Entries = {
        {"ALPHA1", 12500}, {"BRAVO2", 11800}, {"CHARL3", 11200}, {"DELTA4", 10900},
        {"ECHO55", 10450}, {"FOXT66", 10000}, {"GOLF77", 9650},  {"HOTEL8", 9400},
        {"INDIA9", 9150},  {"JULI10", 8900},  {"KILO11", 8700},  {"LIMA12", 8450},
        {"MIKE13", 8300},  {"NOVA14", 8100},  {"OSCAR5", 7900},  {"PAPA16", 7700},
        {"QUE017", 7550},  {"ROMEO8", 7400},  {"SIERA9", 7200},  {"TANGO0", 7050},
        {"UNIF21", 6900},  {"VICT22", 6700},  {"WHIS23", 6550},  {"XRAY24", 6400},
        {"YANKE5", 6200},  {"ZULU26", 6050},  {"AAA027", 5900},  {"BBB028", 5750},
        {"CCC029", 5600},  {"DDD030", 5450}};

    int CurrentPage = 0;
    EUIPopupAction PendingAction = EUIPopupAction::None;
};