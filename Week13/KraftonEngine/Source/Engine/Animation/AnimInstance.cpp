#include "AnimInstance.h"
#include "Animation/AnimationStateMachine.h"
#include "Object/ObjectFactory.h"
#include "Component/SkeletalMeshComponent.h"

IMPLEMENT_CLASS(UAnimInstance, UObject)

void UAnimInstance::Initialize(USkeletalMeshComponent* InOwner, const FString& InScriptPath)
{
	OwnerComponent = InOwner;

	if (!InScriptPath.empty())
	{
		UObject* SMObj = FObjectFactory::Get().Create("ULuaAnimStateMachine");
		StateMachine = Cast<UAnimationStateMachine>(SMObj); // 부모 클래스나 인터페이스로 캐스팅

		if (StateMachine)
		{
			StateMachine->Initialize(InOwner, this);
			StateMachine->LoadScript(InScriptPath);
		}
	}
}

void UAnimInstance::Update(float DeltaTime)
{
	NativeUpdateAnimation(DeltaTime);

	if (StateMachine)
	{
		StateMachine->UpdateAnimationState(DeltaTime);
	}
}

void UAnimInstance::GetCurrentPose(FPoseContext& OutPose)
{
	if (!StateMachine)
		return;

	TArray<FAnimNotifyEvent> CollectedNotifies;
	StateMachine->GenerateFinalPose(OutPose, CollectedNotifies);

	LastEvaluatedTime = StateMachine->GetCurrentStateTime();

	for (const FAnimNotifyEvent& Notify : CollectedNotifies)
		RouteNotify(Notify);
}

void UAnimInstance::TriggerAnimNotifies()
{
	if (!OwnerComponent)
		return;

	for (const FAnimNotifyEvent& Notify : NotifyQueue)
		OwnerComponent->HandleAnimNotify(Notify);
	NotifyQueue.clear();

	for (auto it = ActiveStateNotifies.begin(); it != ActiveStateNotifies.end(); )
	{
		float EndTime = it->Notify.TriggerTime + it->Notify.Duration;
		if (LastEvaluatedTime >= it->Notify.TriggerTime && LastEvaluatedTime <= EndTime)
		{
			OwnerComponent->HandleAnimNotify(it->Notify);
			++it;
		}
		else
		{
			it = ActiveStateNotifies.erase(it);
		}
	}
}

void UAnimInstance::RouteNotify(const FAnimNotifyEvent& Notify)
{
	if (!Notify.IsStateNotify())
	{
		NotifyQueue.push_back(Notify);
		return;
	}

	for (const FActiveNotifyState& Active : ActiveStateNotifies)
	{
		if (Active.Notify.NotifyName == Notify.NotifyName)
			return;
	}
	ActiveStateNotifies.push_back({ Notify });
}

void UAnimInstance::ResetNotifyState()
{
	NotifyQueue.clear();
	ActiveStateNotifies.clear();
}
