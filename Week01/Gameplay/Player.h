#pragma once

#include "Actor.h"
#include <optional>

class FPlayer : public FActor
{
  public:
    EActorType GetActorType() const override;

    void Update(float DeltaTime, FGameContext &Context) override;
    void OnBeat(FStage &Stage) override;

    int  GetLastMovedBeatIndex();
    void SetLastMovedBeatIndex(int CurrentBeatIndex);
    void QueueInput(EDirection InDirection);
    void ClearQueuedInput();
    bool HasQueuedInput() const;

  private:
    std::optional<EDirection> QueuedInput;

    // 한 비트 내 중복 이동 방지용
    int LastMovedBeatIndex = -1;
};
