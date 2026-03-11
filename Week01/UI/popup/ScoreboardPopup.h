#pragma once

#include <string>
#include <vector>
#include "UIPopupAction.h"
#include "UIPopupBase.h"
#include "Data/ScoreRecord.h"

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
    std::vector<FScoreRecord> Entries;
    int                       CurrentPage = 0;
    EUIPopupAction            PendingAction = EUIPopupAction::None;
};
