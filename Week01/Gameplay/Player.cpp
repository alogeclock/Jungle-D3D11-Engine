#include "Player.h"
#include "Stage.h"

EActorType FPlayer::GetActorType() const
{
	return EActorType::Player;
}

void FPlayer::Update(float DeltaTime)
{
	FActor::Update(DeltaTime);
}

void FPlayer::OnBeat(FStage& Stage)
{
	if (HasQueuedInput())
	{
		TryMove(Stage, QueuedInput.value());
		ClearQueuedInput();
	}
}

void FPlayer::QueueInput(EDirection InDirection)
{
	QueuedInput = InDirection;
}

void FPlayer::ClearQueuedInput()
{
	QueuedInput.reset();
}

bool FPlayer::HasQueuedInput() const
{
	return QueuedInput.has_value();
}
