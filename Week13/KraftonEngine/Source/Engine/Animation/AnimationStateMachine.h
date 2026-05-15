#pragma once
#include "Object/Object.h"

class USkeletalMeshComponent;
class UAnimSequence;

class UAnimationStateMachine : public UObject
{
public:
	virtual void Initialize(USkeletalMeshComponent* InOwner) { OwnerComponent = InOwner; }

	// 각 객체마다 전이 조건과 원하는 애니메이션이 다르기 때문에 virtual로
	virtual void ProcessState(float DeltaSeconds) = 0; 

	UAnimSequence* GetCurrentSequence() const;
	float          GetCurrentStateTime() const;

protected:
	USkeletalMeshComponent* OwnerComponent = nullptr;
	int32 CurrentState = 0;
	int32 PrevState = -1;
	float StateLocalTime = 0.0f;
	float BlendAlpha = 1.0f;
	float BlendDuration = 0.2f;
};
