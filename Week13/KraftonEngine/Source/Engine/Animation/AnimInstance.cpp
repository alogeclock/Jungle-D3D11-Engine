#include "AnimInstance.h"
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
	TriggerAnimNotifies();
}

void UAnimInstance::CheckAnimNotifyQueue(float PrevTime, float CurrTime,
	float SeqLength, bool bLooping, bool bReverse,
	const TArray<FAnimNotifyEvent>& SeqNotifies)
{
	NotifyQueue.clear();

	for (const FAnimNotifyEvent& Notify : SeqNotifies)
	{
		bool bShouldTrigger = false;

		if (!bReverse)
		{
			// 정방향 재생
			if (bLooping && CurrTime < PrevTime)
			{
				// 루프 발생: [PrevTime, SeqLength] ∪ [0, CurrTime]
				bShouldTrigger = (Notify.TriggerTime > PrevTime && Notify.TriggerTime <= SeqLength)
				              || (Notify.TriggerTime >= 0.f     && Notify.TriggerTime <= CurrTime);
			}
			else
			{
				bShouldTrigger = Notify.TriggerTime > PrevTime && Notify.TriggerTime <= CurrTime;
			}
		}
		else
		{
			// 역방향 재생
			if (bLooping && CurrTime > PrevTime)
			{
				// 역방향 루프 발생: [PrevTime, 0] ∪ [SeqLength, CurrTime]
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
