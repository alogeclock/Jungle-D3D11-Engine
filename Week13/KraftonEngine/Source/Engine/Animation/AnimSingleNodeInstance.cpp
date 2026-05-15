#include "AnimSingleNodeInstance.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UAnimSingleNodeInstance, UAnimInstance)

void UAnimSingleNodeInstance::Initialize(USkeletalMeshComponent* InOwner)
{
	Super::Initialize(InOwner);
	CurrentTime = 0.0f;
	bPlaying = false;
}

void UAnimSingleNodeInstance::SetAnimation(UAnimationAsset* Asset)
{
	Sequence = Cast<UAnimSequence>(Asset);
	CurrentTime = 0.0f;
	bPlaying = false;
}

void UAnimSingleNodeInstance::Play(bool bInLooping)
{
	if (!Sequence)
		return;

	bLooping = bInLooping;
	bPlaying = true;
}

void UAnimSingleNodeInstance::Stop()
{
	bPlaying = false;
}

void UAnimSingleNodeInstance::SetPlayRate(float Rate)
{
	PlayRate = Rate;
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (!bPlaying || !Sequence)
		return;

	float PrevTime = CurrentTime;
	CurrentTime += DeltaSeconds * PlayRate;

	// float Length = Sequence->GetLength();
	// if (bLooping)
	// {
	//     while (CurrentTime >= Length) CurrentTime -= Length;
	// }
	// else if (CurrentTime >= Length)
	// {
	//     CurrentTime = Length;
	//     bPlaying = false;
	// }
	// CheckAnimNotifyQueue(PrevTime, CurrentTime, Length, bLooping, PlayRate < 0.f,
	//                      Sequence->GetNotifyEvents());
}

void UAnimSingleNodeInstance::GetCurrentPose(FPoseContext& OutPose) const
{
	if (!Sequence)
		return;

	// FAnimationRuntime::GetPoseAtTime(Sequence->GetDataModel(), CurrentTime, OutPose);
}
