#pragma once

#include "Core/GameContext.h"
#include "Gameplay/BeatSystem.h"
#include "Render/Texture.h"
#include "UIWidget.h"
#include <vector>
#include "BeatEffect.h"

class FBeatHUDWidget : public IUIWidget
{
  public:
    FBeatHUDWidget() : BeatEffects(10) {}
    void BindBeatSystem(const FBeatSystem *BeatSystem);

    void         Update(FGameContext &Context) override;
    void         Render(FGameContext &Context) override;
    virtual void SetTextures(FGameContext &Context) override;

    // 판정 결과를 외부에서 주입받는 함수 추가
    void OnBeatJudged(EBeatJudge Judge, float InScore, int InCombo);

    BeatEffect *GetFromPool(FTexture *Texture, float PosX, float PosY, float InLifetime,
                            float InScore, int InCombo);

  private:
    const FBeatSystem *BeatSystem = nullptr;
    float              MaxDistance = 400.f;
    FVec2              Heart;
    const float        OrgHeartXScale = 120.f;
    const float        OrgHeartYScale = 150.f;
    float              HeartXScale = OrgHeartXScale;
    float              HeartYScale = OrgHeartYScale;

    FTexture *HeartTexture = nullptr;
    FTexture *BarWidthTexture = nullptr;
    FTexture *BarTexture = nullptr;
    FTexture *PerfectTexture = nullptr;
    FTexture *GoodTexture = nullptr;
    FTexture *MissTexture = nullptr;
    float     Ypos = 0;

    int Combo = 0;

    std::vector<BeatEffect> BeatEffects;
};
