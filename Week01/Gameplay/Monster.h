#pragma once

#include "Actor.h"
#include <optional>

class FMonster : public FActor
{
  public:
    EActorType GetActorType() const override;

    void Update(float DeltaTime) override;
    void OnBeat(FStage &Stage) override;

    void           SetAiType(EMonsterAIType InAiType);
    EMonsterAIType GetAiType() const;

    EDirection DecideNextMove(const FStage &Stage) const;

  private:
    std::optional<EDirection> SearchPlayer(const FStage &Stage) const;

    EMonsterAIType AiType = EMonsterAIType::RandomMove;
    int            SearchRange = 20; // 플레이어 탐색 최대 범위 (20스텝)
};
