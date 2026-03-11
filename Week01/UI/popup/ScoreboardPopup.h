#pragma once

#include <vector>
#include "Data/ScoreRepository.h"
#include "UIPopupBase.h"

struct FGameContext;

class FScoreboardPopup : public FUIPopupBase
{
  public:
    void SetEntries(const std::vector<FScoreRecord> &InEntries);

    EUIPopupAction ConsumeAction();
    void            Render(FGameContext &Context) override;
    void            Update(FGameContext &Context) override {}

    void GoToNextPage();
    void ResetPage();

  private:
    int GetTotalPages() const;
    int GetPageStartIndex() const;
    int GetPageEntryCount() const;

    void DrawBottomButtons(const FPopupFrameLayout &Layout, bool bHasPrevPage, bool bHasNextPage);
    void DrawEntries(const FPopupFrameLayout &Layout);
    void DrawPageText(const FPopupFrameLayout &Layout);
    void DrawColumnBlock(const FPopupFrameLayout &Layout, float BaseX, int StartEntryIndex, int EntryCount);
    void DrawColumnHeader(float BaseX, float Y);
    void DrawRow(float BaseX, float Y, int Rank, const FScoreRecord &Entry);

  private:
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Small;

    static constexpr int EntriesPerPage = 10;
    static constexpr int MaxRowsPerColumn = 5;

    static constexpr float ColumnGap = 36.0f;
    static constexpr float HeaderToRowsGap = 12.0f;
    static constexpr float RowGap = 10.0f;

    static constexpr float TableTopOffset = 10.0f;
    static constexpr float HeaderLineOffset = 4.0f;
    static constexpr float ColumnDividerPadding = 8.0f;

    static constexpr float RankColumnWidth = 90.0f;
    static constexpr float NameColumnWidth = 90.0f;
    static constexpr float StageColumnWidth = 90.0f;
    static constexpr float ScoreColumnWidth = 84.0f;

  private:
    std::vector<FScoreRecord> Entries;
    int                       CurrentPage = 0;
    EUIPopupAction            PendingAction = EUIPopupAction::None;
};
