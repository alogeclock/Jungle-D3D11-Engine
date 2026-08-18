#pragma once

#include "Core/GameContext.h"
#include "Core/Types.h"
#include "UIWidget.h"
#include "Render/Texture.h"
#include "Core/Types.h"
#include "UI/widget/BeatEffect.h"
class FStage;

class FGameplayHUDWidget : public IUIWidget
{
  public:
    void BindStage(const FStage *Stage);
    void BindPauseFlag(bool *InPauseFlag);

    void         Update(FGameContext &Context) override;
    void         Render(FGameContext &Context) override;
    virtual void SetTextures(FGameContext &Context) override;

    void OnBeatScoreUpdate(int InScore);
    void OnTimerBonusUpdate(int InScore);
    void ResetPlayTime();

  private:
    const FStage *Stage = nullptr;
    bool         *PauseFlag = nullptr;
    float         PlayTime = 0.0f;
    int           MaxHPToDisplay = 5;
    FTexture     *LifeTexture = nullptr;
    FTexture     *LifeDeadTexture = nullptr;
    FVec2         HPTextPos = {700.0f, 20.0f};
    float         HeartScale = 50.f;

    // Score 애니메이션
    int   DisplayScore = 0;
    int   TargetScore = 0;
    int   ScoreAnimStart = 0;
    float ScoreAnimTimer = 0.0f;
    float ScoreAnimDuration = 1.0f;
};
