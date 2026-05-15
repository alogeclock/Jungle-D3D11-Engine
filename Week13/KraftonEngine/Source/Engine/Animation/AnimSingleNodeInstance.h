#pragma once
#include "Animation/AnimInstance.h"
#include "Animation/AnimType.h"

class UAnimationAsset;
class UAnimSequence;

class UAnimSingleNodeInstance : public UAnimInstance
{
public:
	DECLARE_CLASS(UAnimSingleNodeInstance, UAnimInstance)

	void SetAnimation(UAnimationAsset* Asset);
	void Play(bool bLooping);
	void Stop();
	void SetPlayRate(float Rate);

	void Initialize(USkeletalMeshComponent* InOwner) override;
	void NativeUpdateAnimation(float DeltaSeconds) override;
	void GetCurrentPose(FPoseContext& OutPose) const override;

private:
	UAnimSequence* Sequence = nullptr;
	float CurrentTime = 0.0f;
	float PlayRate = 1.0f;
	bool  bLooping = false;
	bool  bPlaying = false;
};
