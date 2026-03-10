#include "pch.h"
#include "Player.h"
#include "Stage.h"

EActorType FPlayer::GetActorType() const { return EActorType::Player; }

void FPlayer::Update(float DeltaTime, FGameContext &Context) { FActor::Update(DeltaTime, Context); }

void FPlayer::OnBeat(FStage &Stage)
{
    if (HasQueuedInput())
    {
        TryMove(Stage, QueuedInput.value());
        ClearQueuedInput();
    }
}

int FPlayer::GetLastMovedBeatIndex() { return LastMovedBeatIndex; }

void FPlayer::SetLastMovedBeatIndex(int CurrentBeatIndex) { LastMovedBeatIndex = CurrentBeatIndex; }

void FPlayer::QueueInput(EDirection InDirection) { QueuedInput = InDirection; }

void FPlayer::ClearQueuedInput() { QueuedInput.reset(); }

bool FPlayer::HasQueuedInput() const { return QueuedInput.has_value(); }
