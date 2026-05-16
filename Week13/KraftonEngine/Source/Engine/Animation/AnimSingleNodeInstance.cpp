#include "AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Object/ObjectFactory.h"
#include <algorithm>

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

	++TickCount;
	float PrevTime = CurrentTime;
	CurrentTime += DeltaSeconds * PlayRate;

	float Length = Sequence->GetPlayLength();
	if (bLooping)
	{
		while (CurrentTime >= Length) CurrentTime -= Length;
	}
	else if (CurrentTime >= Length)
	{
		CurrentTime = Length;
		bPlaying = false;
	}

	CheckAnimNotifyQueue(PrevTime, CurrentTime, Length, bLooping, PlayRate < 0.f,
		Sequence->GetNotifyEvents());
}

void UAnimSingleNodeInstance::SetCurrentTime(float InTime)
{
	float Length = Sequence ? Sequence->GetPlayLength() : 0.0f;
	CurrentTime = (Length > 0.0f) ? std::clamp(InTime, 0.0f, Length) : 0.0f;
}

void UAnimSingleNodeInstance::GetCurrentPose(FPoseContext& OutPose)
{
	if (!Sequence)
		return;

	TArray<FMatrix> LocalMatrices;
	if (!Sequence->EvaluatePose(CurrentTime, LocalMatrices))
		return;

	OutPose.BoneLocalTransforms.resize(LocalMatrices.size());
	for (size_t i = 0; i < LocalMatrices.size(); ++i)
		OutPose.BoneLocalTransforms[i] = FTransform(LocalMatrices[i]);
}
