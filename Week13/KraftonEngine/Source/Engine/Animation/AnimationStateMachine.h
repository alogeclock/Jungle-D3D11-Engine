#pragma once
#include "Object/Object.h"
#include "Animation/AnimTypes.h"

class USkeletalMeshComponent;
class UAnimSequence;

class UAnimationStateMachine : public UObject
{
public:
	virtual void Initialize(USkeletalMeshComponent* InOwner) { OwnerComponent = InOwner; }

	// PrevStateLocalTime 저장 → BlendAlpha 갱신 → ProcessState() 호출
	void Tick(float DeltaTime);

	// 각 구현체가 전이 조건과 StateLocalTime을 관리
	virtual void ProcessState(float DeltaSeconds) = 0;

	// 포즈 생성 + 해당 프레임 구간의 Notify 수집
	virtual void EvaluatePose(FPoseContext& OutPose, TArray<FAnimNotifyEvent>& OutNotifies) const;

	UAnimSequence* GetCurrentSequence() const;
	float GetCurrentStateTime() const;

protected:
	USkeletalMeshComponent* OwnerComponent  = nullptr;
	UAnimSequence*          CurrentSequence = nullptr;
	UAnimSequence*          PrevSequence    = nullptr;

	float PrevStateLocalTime = 0.0f;
	float PrevStateTime      = 0.0f;

	int32 CurrentState   = 0;
	int32 PrevState      = -1;
	float StateLocalTime = 0.0f;
	float BlendAlpha     = 1.0f;
	float BlendDuration  = 0.2f;
};
