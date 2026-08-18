#pragma once

#include "UIPopupAction.h"
#include "UIPopupBase.h"

class FStageClearPopup : public FUIPopupBase
{
  public:
    void SetData(bool bInAllCleared, int InClearedStage)
    {
        bAllCleared = bInAllCleared;
        ClearedStage = InClearedStage;
    }

    EUIPopupAction ConsumeAction();
    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    static constexpr EUIPopupContentAlign ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Big;
    static constexpr float MessageLineGap = 12.0f;

  private:
    void DrawAllClearedMessage(const FPopupFrameLayout &Layout);
    void DrawNormalClearMessage(const FPopupFrameLayout &Layout);

  private:
    bool bAllCleared = false;
    int ClearedStage = 0;
    EUIPopupAction PendingAction = EUIPopupAction::None;
};
