#pragma once

#include "Core/GameContext.h"
#include "Actor.h"
#include <optional>

struct FGameContext;

class FMonster : public FActor
{
  public:
    EActorType GetActorType() const override;

    void Update(float DeltaTime, FGameContext &Context) override;
    void OnBeat(FStage &Stage) override;

    void           SetMoveFrequency(int InFreq);
    int            GetMoveFrequency() const;
    void           SetAiType(EMonsterAIType InAiType);
    EMonsterAIType GetAiType() const;
    void           SetMonsterType(int InMonType);
    int            GetMonsterType() const;
    void           SetSearchRange(int InSearchRange);
    int            GetSearchRange() const;

    EDirection DecideNextMove(const FStage &Stage) const;

  private:
    std::optional<EDirection> SearchPlayer(const FStage &Stage) const;

    EMonsterAIType AiType = EMonsterAIType::RandomMove;
    int            MonsterType = 0;
    int            MoveFrequency = 2;
    int            MoveFrequencyOffset = 0;
    int            BeatCount = 0;
    int            SearchRange = 5; // ÇÃ·¹ÀÌ¾î Å½»ö ÃÖ´ë ¹üÀ§ (20½ºÅÜ)
};
