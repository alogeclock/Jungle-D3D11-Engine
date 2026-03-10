#pragma once

#include "Core/GameContext.h"
#include "UIWidget.h"

struct FStage;

class FGameplayHUDWidget : public IUIWidget
{
  public:
    void BindStage(const FStage *Stage);

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

    void ResetPlayTime();

  private:
    const FStage *Stage = nullptr;
    float         PlayTime = 0.0f;
    int           MaxHPToDisplay = 5;
};
