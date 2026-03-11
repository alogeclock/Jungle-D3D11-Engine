#pragma once

#include "Core/GameContext.h"
#include "Actor.h"
#include <optional>

struct FGameContext;

enum class EMonsterType
{
    StoneGolem,
    FireGolem
};

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
    void           SetMonsterType(EMonsterType InMonsterType);
    EMonsterType   GetMonsterType() const;
    void           SetSearchRange(int InSearchRange);
    int            GetSearchRange() const;
    std::string    GetMonsterTextureKey(EMonsterType Type);

    EDirection DecideNextMove(const FStage &Stage) const;

  private:
    std::optional<EDirection> SearchPlayer(const FStage &Stage) const;

    EMonsterAIType AiType = EMonsterAIType::RandomMove;
    EMonsterType   MonsterType = EMonsterType::StoneGolem;
    int            MoveFrequency = 2;
    int            MoveFrequencyOffset = 0;
    int            BeatCount = 0;
    int            SearchRange = 5; // 탐색 반경 (BFS 검색을 수행할 최대 범위)
};
