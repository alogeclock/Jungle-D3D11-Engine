#pragma once

#include "Actor.h"
#include "Item.h"
#include <optional>
#include <vector>

class FPlayer : public FActor
{
  public:
    EActorType GetActorType() const override;

    void Update(float DeltaTime, FGameContext &Context) override;
    void OnBeat(FStage &Stage) override;
    void Damage(int Amount) override;

    int  GetLastMovedBeatIndex();
    void SetLastMovedBeatIndex(int CurrentBeatIndex);
    void QueueInput(EDirection InDirection);
    void ClearQueuedInput();
    bool HasQueuedInput() const;

    // 플레이어 전용 아이템 효과 (Invincibility, TimeScaleDown)
    void                              AddEffect(const FActiveEffect &Effect);
    void                              UpdateActiveEffects(float DeltaTime);
    const std::vector<FActiveEffect> &GetActiveEffects() const;
    bool                              HasActiveEffect(EItemType Type) const;
    bool                              ConsumeInvincibility();

  private:
    std::optional<EDirection> QueuedInput;

    // 한 비트 내 중복 이동 방지용
    int   LastMovedBeatIndex = -1;
    float HitInvincibilityTimer = 0.0f;

    std::vector<FActiveEffect> ActiveEffects;
};
