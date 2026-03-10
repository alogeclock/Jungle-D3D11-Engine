#pragma once

#include "Core/GameContext.h"
#include "Gameplay/BeatSystem.h"
#include "UIWidget.h"

class FBeatHUDWidget : public IUIWidget
{
  public:
    void BindBeatSystem(const FBeatSystem *BeatSystem);

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    const FBeatSystem *BeatSystem = nullptr;
    float              FlashTimer = 0.0f;
    float              FlashDuration = 0.08f;
};
