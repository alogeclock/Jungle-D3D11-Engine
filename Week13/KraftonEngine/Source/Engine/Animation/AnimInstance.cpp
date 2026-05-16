#include "AnimInstance.h"
#include "Animation/AnimationStateMachine.h"
#include "Object/ObjectFactory.h"
#include "Component/SkeletalMeshComponent.h"

IMPLEMENT_CLASS(UAnimInstance, UObject)

void UAnimInstance::Initialize(USkeletalMeshComponent* InOwner)
{
	OwnerComponent = InOwner;
}

void UAnimInstance::Update(float DeltaTime)
{
	NativeUpdateAnimation(DeltaTime);

	if (StateMachine)
	{
		StateMachine->Tick(DeltaTime);
	}
}

void UAnimInstance::GetCurrentPose(FPoseContext& OutPose)
{
	if (!StateMachine)
		return;

	TArray<FAnimNotifyEvent> CollectedNotifies;
	StateMachine->EvaluatePose(OutPose, CollectedNotifies);

	for (FAnimNotifyEvent& Notify : CollectedNotifies)
		NotifyQueue.push_back(Notify);
}

void UAnimInstance::CheckAnimNotifyQueue(float PrevTime, float CurrTime,
	float SeqLength, bool bLooping, bool bReverse,
	const TArray<FAnimNotifyEvent>& SeqNotifies)
{
	for (const FAnimNotifyEvent& Notify : SeqNotifies)
	{
		bool bShouldTrigger = false;

		if (!bReverse)
		{
			if (bLooping && CurrTime < PrevTime)
			{
				bShouldTrigger = (Notify.TriggerTime > PrevTime && Notify.TriggerTime <= SeqLength)
				              || (Notify.TriggerTime <= CurrTime);
			}
			else
			{
				bShouldTrigger = Notify.TriggerTime > PrevTime && Notify.TriggerTime <= CurrTime;
			}
		}
		else
		{
			if (bLooping && CurrTime > PrevTime)
			{
				bShouldTrigger = (Notify.TriggerTime < PrevTime  && Notify.TriggerTime >= 0.f)
				              || (Notify.TriggerTime <= SeqLength && Notify.TriggerTime >= CurrTime);
			}
			else
			{
				bShouldTrigger = Notify.TriggerTime < PrevTime && Notify.TriggerTime >= CurrTime;
			}
		}

		if (bShouldTrigger)
			NotifyQueue.push_back(Notify);
	}
}

void UAnimInstance::TriggerAnimNotifies()
{
	if (!OwnerComponent || NotifyQueue.empty())
		return;

	for (const FAnimNotifyEvent& Notify : NotifyQueue)
		OwnerComponent->HandleAnimNotify(Notify);

	NotifyQueue.clear();
}
