#pragma once

#include "Core/GameContext.h"
#include "Core/Types.h"
#include "UIWidget.h"

class FStage;

class FGameplayHUDWidget : public IUIWidget
{
  public:
    void BindStage(const FStage *Stage);
    void BindPauseFlag(bool *InPauseFlag);

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

    // 판정 결과를 외부에서 주입받는 함수 추가
    void OnBeatJudged(EBeatJudge Judge);

    void ResetPlayTime();

  private:
    const FStage *Stage = nullptr;
    bool         *PauseFlag = nullptr;
    float         PlayTime = 0.0f;
    int           MaxHPToDisplay = 5;
};
