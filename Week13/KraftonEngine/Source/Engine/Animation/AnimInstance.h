#pragma once
#include "Object/Object.h"
#include "Animation/AnimType.h"

class UAnimationStateMachine;
class USkeletalMeshComponent;

class UAnimInstance : public UObject
{
public:
	DECLARE_CLASS(UAnimInstance, UObject)
	virtual void Initialize(USkeletalMeshComponent* InOwner);
	void Update(float DeltaTime);
	virtual void NativeUpdateAnimation(float DeltaSeconds) {}

	void CheckAnimNotifyQueue(float PrevTime, float CurrTime,
		float SeqLength, bool bLooping, bool bReverse,
		const TArray<FAnimNotifyEvent>& SeqNotifies);
	void TriggerAnimNotifies();

	virtual void GetCurrentPose(FPoseContext& OutPose) const {}

	void SetStateMachine(UAnimationStateMachine* SM) { StateMachine = SM; }

	USkeletalMeshComponent* GetOwnerComponent() const { return OwnerComponent; }

protected:
	USkeletalMeshComponent* OwnerComponent = nullptr;
	UAnimationStateMachine* StateMachine = nullptr;

private:
	TArray<FAnimNotifyEvent> NotifyQueue;
};
