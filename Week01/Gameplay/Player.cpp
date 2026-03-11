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

void FPlayer::AddEffect(const FActiveEffect &Effect)
{
    ActiveEffects.push_back(Effect);
}

void FPlayer::UpdateActiveEffects(float DeltaTime)
{
    for (auto It = ActiveEffects.begin(); It != ActiveEffects.end();)
    {
        // Invincibility: 1회용, 시간 만료 없음 (피격 시 소모)
        if (It->Type == EItemType::Invincibility)
        {
            ++It;
            continue;
        }

        It->RemainingTime -= DeltaTime;
        if (It->RemainingTime <= 0.0f)
        {
            It = ActiveEffects.erase(It);
        }
        else
        {
            ++It;
        }
    }
}

const std::vector<FActiveEffect> &FPlayer::GetActiveEffects() const { return ActiveEffects; }

bool FPlayer::HasActiveEffect(EItemType Type) const
{
    for (const auto &Effect : ActiveEffects)
    {
        if (Effect.Type == Type)
            return true;
    }
    return false;
}

bool FPlayer::ConsumeInvincibility()
{
    for (auto It = ActiveEffects.begin(); It != ActiveEffects.end(); ++It)
    {
        if (It->Type == EItemType::Invincibility)
        {
            ActiveEffects.erase(It);
            return true;
        }
    }
    return false;
}
