#pragma once
#include "Object/Object.h"
#include "Animation/AnimTypes.h"

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

	// SkeletalMeshComponent가 호출 — 포즈 생성 및 Notify 수집
	virtual void GetCurrentPose(FPoseContext& OutPose);

	void SetStateMachine(UAnimationStateMachine* SM) { StateMachine = SM; }

	USkeletalMeshComponent* GetOwnerComponent() const { return OwnerComponent; }

protected:
	USkeletalMeshComponent* OwnerComponent  = nullptr;
	UAnimationStateMachine* StateMachine    = nullptr;
	float                   LastEvaluatedTime = 0.0f;

	void ResetNotifyState();

private:
	struct FActiveNotifyState
	{
		FAnimNotifyEvent Notify;
	};

	TArray<FAnimNotifyEvent>    NotifyQueue;
	TArray<FActiveNotifyState>  ActiveStateNotifies;

	void RouteNotify(const FAnimNotifyEvent& Notify);
};