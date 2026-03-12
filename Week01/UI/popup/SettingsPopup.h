#pragma once

#include "UIPopupAction.h"
#include "UIPopupBase.h"

struct FGameContext;

class FSettingsPopup : public FUIPopupBase
{
  public:
    void LoadCurrentVolumes();

    EUIPopupAction ConsumeAction();
    void           Render(FGameContext &Context) override;
    void           Update(FGameContext &Context) override {}

  private:
    void DrawVolumeSliders(const FPopupFrameLayout &Layout);

    float MasterVolume = 1.0f;
    float BgmVolume = 1.0f;
    float SfxVolume = 1.0f;

    EUIPopupAction PendingAction = EUIPopupAction::None;
};
